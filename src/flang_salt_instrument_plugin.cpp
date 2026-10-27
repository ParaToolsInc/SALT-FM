/*
Copyright (C) 2024-2025, ParaTools, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

/* SALT-FM Flang Fortran Instrumentor Plugin */

// See https://flang.llvm.org/docs/FlangDriver.html#frontend-driver-plugins
// for documentation of the Flang frontend plugin interface

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <optional>
#include <tuple>
#include <algorithm>
#include <filesystem>


#include <dprint.hpp>
#include <ryml_all.hpp>
#include "config_file.hpp"
#include "env_bool.hpp"
#include "output_file.hpp"

// On macOS, ryml_all.hpp transitively includes <sys/param.h>, which
// defines MAX(a,b) and MIN(a,b) as function-like macros.  Those collide
// with the MAX/MIN member functions of Fortran::evaluate::value::Integer
// (flang/Evaluate/integer.h, included via flang/Evaluate/tools.h),
// surfacing as "too few arguments provided to function-like macro
// invocation" on the Integer member declarations.  Undef before pulling
// in the flang headers below.
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif

#include <clang/Basic/SourceLocation.h>

#include "flang/Frontend/FrontendActions.h"
#include "flang/Frontend/FrontendPluginRegistry.h"
#include "flang/Parser/dump-parse-tree.h"
#include "flang/Parser/parsing.h"
#include "flang/Parser/tools.h"
#include "flang/Parser/source.h"
#include "flang/Common/indirection.h"
#include "flang/Semantics/symbol.h"
#include "flang/Evaluate/tools.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Path.h"
#include "flang/Frontend/CompilerInstance.h"

#include "flang_instrumentation_constants.hpp"
#include "selectfile.hpp"
#include "flang_source_location.hpp"
#include "flang_instrumentation_point.hpp"

using namespace std::string_literals;
using namespace Fortran::frontend;

/**
 * The main action of the Salt instrumentor.
 * Visits each node in the parse tree.
 */
namespace salt::fortran {
    // The file name of `pos` as written: Flang doubles each `\` of the file name
    // of a `#line` directive, but not of the path of a source file
    static std::string writtenPath(const Fortran::parser::SourcePosition &pos) {
        // shortcut: a `#line` name with one `\` where the source path has `\\`
        // doubles to the source path and stays doubled; upgrade if Flang keeps
        // whether a name came from a `#line` directive
        if (*pos.path == pos.sourceFile->path()) {
            return *pos.path;
        }
        std::string name;
        for (std::size_t i{0}; i < pos.path->size(); ++i) {
            name += (*pos.path)[i];
            if ((*pos.path)[i] == '\\' && i + 1 < pos.path->size() && (*pos.path)[i + 1] == '\\') {
                ++i;
            }
        }
        return name;
    }

    class SaltInstrumentAction final : public PluginParseTreeAction {
        struct SaltInstrumentParseTreeVisitor {
            explicit SaltInstrumentParseTreeVisitor(Fortran::parser::Parsing *parsing,
                                                    const bool skipInstrument = false,
                                                    const bool mainSetup = true)
                : mainProgramLine_(0), subProgramLine_(0), skipInstrumentFile_(skipInstrument),
                  mainSetup_(mainSetup), parsing(parsing) {
            }

            bool shouldInstrument() const {
                return !skipInstrumentFile_ && !skipInstrumentSubprogram_;
            }

            void addProgramBeginInstrumentation(const int start_line, const std::string &timer_name) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(
                        std::make_unique<ProgramBeginInstrumentationPoint>(start_line, timer_name));
                }
            }

            // The setup code of a main program whose timer the SIF removes,
            // even when the file lists exclude its file
            void addProgramSetupInstrumentation(const int start_line, const std::string &timer_name) {
                instrumentationPoints_.emplace_back(std::make_unique<ProgramBeginInstrumentationPoint>(
                    start_line, timer_name, InstrumentationPointType::PROGRAM_SETUP));
            }

            void addProcedureBeginInstrumentation(const int start_line, const std::string &timer_name) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(
                        std::make_unique<ProcedureBeginInstrumentationPoint>(start_line, timer_name));
                }
            }

            void addProcedureEndInstrumentation(const int end_line) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(std::make_unique<ProcedureEndInstrumentationPoint>(end_line));
                }
            }

            // The input positions of a statement and its label, as
            // ReturnInstrumentationPoint takes them
            struct StatementPositions {
                InputPosition start;
                InputPosition end;
                std::optional<InputPosition> next;
                std::optional<StatementLabel> label;
            };

            // The timer stop directly before the End statement at `pos`
            void addProcedureEndInstrumentation(const StatementPositions &pos) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(std::make_unique<ReturnInstrumentationPoint>(
                        InstrumentationPointType::PROCEDURE_END, pos.start, pos.end, pos.next, pos.label));
                }
            }

            void addReturnStmtInstrumentation(const StatementPositions &pos) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(std::make_unique<ReturnStmtInstrumentationPoint>(
                        pos.start, pos.end, pos.next, pos.label));
                }
            }

            void addIfReturnStmtInstrumentation(const StatementPositions &pos, const InputPosition returnStart,
                                                const bool returnSplit) {
                if (shouldInstrument()) {
                    instrumentationPoints_.emplace_back(std::make_unique<IfReturnStmtInstrumentationPoint>(
                        pos.start, pos.end, pos.next, returnStart, returnSplit));
                }
            }

            // The input position of the start, or with `end` the end, of
            // `chars`.
            //
            // If locationFromSource fails to map a CharBlock we DIE rather
            // than skip: dropping a single instrumentation point would leave
            // a TAU_PROFILE_TIMER_START without a matching TAU_PROFILE_STOP
            // at runtime.
            InputPosition inputPosition(const Fortran::parser::CharBlock &chars, const bool end) const {
                const auto pos{locationFromSource(parsing, chars, end)};
                if (!pos.has_value()) {
                    DIE("SALT: cooked-source mapping unavailable for a return or End statement; "
                        "refusing to emit unbalanced TAU timers");
                }
                return InputPosition{pos->trueLineNumber, pos->column};
            }

            // The input positions of `stmt`, from its label, if any, and of
            // the statement after it on its Fortran line, which the cooked
            // text separates from it with `;`.
            template<typename A>
            StatementPositions statementPositions(const Fortran::parser::Statement<A> &stmt) const {
                const Fortran::parser::CharBlock &source{stmt.source};
                const Fortran::parser::CharBlock cooked{DEREF(parsing->allCooked().Find(source)).AsCharBlock()};
                const auto isBlank = [](const char c) { return c == ' ' || c == '\t'; };
                std::optional<InputPosition> next;
                const char *after{source.end()};
                while (after < cooked.end() && isBlank(*after)) {
                    ++after;
                }
                if (after < cooked.end() && *after == ';') {
                    while (after < cooked.end() && (isBlank(*after) || *after == ';')) {
                        ++after;
                    }
                    if (after < cooked.end() && *after != '\n') {
                        next = inputPosition({after, 1}, false);
                    }
                }
                std::optional<StatementLabel> label;
                if (stmt.label.has_value()) {
                    // The statement goes on after the label and blanks
                    const char *text{source.begin()};
                    while (text < source.end() && ((*text >= '0' && *text <= '9') || isBlank(*text))) {
                        ++text;
                    }
                    label = StatementLabel{static_cast<long>(stmt.label.value()), inputPosition({text, 1}, false)};
                }
                return {inputPosition(source, false), inputPosition(source, true), next, label};
            }

            [[nodiscard]] const auto &getInstrumentationPoints() const {
                return instrumentationPoints_;
            }

            [[nodiscard]] std::string dumpInstrumentationPoints() const {
                std::stringstream ss;
                for (const auto &instPt: getInstrumentationPoints()) {
                    ss << instPt->toString() << "\n";
                }
                return ss.str();
            }

            [[nodiscard]] static bool shouldInstrumentSubprogram(const std::string &subprogramName) {
                // Fortran names ignore case
                return instrumentRoutine(subprogramName, /*ignoreCase=*/true);
            }

            // Default empty visit functions for otherwise unhandled types.
            template<typename A>
            static bool Pre(const A &) { return true; }

            template<typename A>
            static void Post(const A &) {
                // this space intentionally left blank
            }

            // Override all types that we want to visit.

            // Pre occurs when first visiting a node.
            // Post occurs when returning from the node's children.
            // See https://flang.llvm.org/docs/Parsing.html for information on the parse tree.

            // Parse tree types are defined in: include/flang/Parser/parse-tree.h
            // There are three types of parse tree nodes:
            // Wrappers, with a single data member, always named `v`.
            // Tuples, encapsulating multiple values in a data member named `t` of type std::tuple.
            // Discriminated unions, one of several types stored in data member named `u` of type std::variant.
            // Use std::get() to retrieve value from `t` or `u`

            // See https://github.com/llvm/llvm-project/blob/main/flang/examples/FlangOmpReport/FlangOmpReportVisitor.cpp
            // for examples of getting source position for a parse tree node

            // Never descend into InterfaceSpecification nodes, they can't contain executable statements.
            static bool Pre(const Fortran::parser::InterfaceSpecification &) { return false; }

            // Capture the line number that delimits the END of the
            // procedure's executable section, derived from the wrapping
            // subprogram/main-program tuple instead of from the last
            // ExecutionPartConstruct's source.  The last construct's source
            // can fail to map back through CookedSource for certain
            // statements (issue #31: labeled CONTINUE terminating a label-DO
            // at the end of the body, see llvm-project#196291), so we use
            // the wrapper's bookkeeping as the single source of truth.
            //
            // Also captures the column of the last character of the End
            // statement, used by the TAU timer-name source-range field
            // (PDT convention: inclusive last-char column).  Note: flang's
            // GetSourcePositionRange().second.column is one-past-last, so
            // we subtract 1.
            //
            // When an InternalSubprogramPart is present (a `contains`
            // section), the procedure's body ends just before the
            // ContainsStmt; otherwise it ends just before the End statement.
            //
            // endLineOut and containsLineOut are physical lines of the
            // input, where the TAU calls go.  nameEndLineOut is the line
            // of the body's end as `#line` directives number it, for the
            // timer name.  Without a `contains`, endStmtOut gets the input
            // positions of the End statement, where the timer stop goes.
            template <typename SubprogramT, typename EndStmtT>
            void captureBodyEndLines(const SubprogramT &subprogram, int &endLineOut, int &endColOut,
                                     int &containsLineOut, int &nameEndLineOut,
                                     std::optional<StatementPositions> &endStmtOut) {
                // Reset so a procedure that has no `contains` does not inherit
                // its parent's containsLine bookkeeping.  Without this, an
                // internal procedure walked from inside an outer procedure
                // that *does* have a `contains` would see a stale
                // containsLineOut and emit PROCEDURE_END at the outer's
                // boundary -- breaking the line-sorted instrumentation
                // invariant.
                endLineOut = 0;
                endColOut = 1;
                containsLineOut = 0;
                nameEndLineOut = 0;
                endStmtOut.reset();
                const auto &endStmt = std::get<Fortran::parser::Statement<EndStmtT> >(subprogram.t);
                if (const auto pos = locationFromSource(parsing, endStmt.source, false); pos.has_value()) {
                    endLineOut = pos.value().trueLineNumber;
                    nameEndLineOut = pos.value().line;
                }
                if (const auto pos = locationFromSource(parsing, endStmt.source, true); pos.has_value()) {
                    endColOut = pos.value().column > 1 ? pos.value().column - 1 : 1;
                }
                const auto &maybeInternalPart =
                    std::get<std::optional<Fortran::parser::InternalSubprogramPart> >(subprogram.t);
                if (maybeInternalPart.has_value()) {
                    const auto &containsStmt =
                        std::get<Fortran::parser::Statement<Fortran::parser::ContainsStmt> >(
                            maybeInternalPart.value().t);
                    if (const auto pos = locationFromSource(parsing, containsStmt.source, false); pos.has_value()) {
                        containsLineOut = pos.value().trueLineNumber;
                        nameEndLineOut = pos.value().line;
                    }
                } else if (endLineOut > 0) {
                    endStmtOut = statementPositions(endStmt);
                }
            }

            // Capture the column where the subprogram's start statement
            // (subroutine/function/module-procedure keyword or its prefix)
            // begins.  Uses the wrapping `Statement<StartStmtT>::source`
            // from the parent tuple so we get the keyword position rather
            // than the procedure name's column.
            template <typename SubprogramT, typename StartStmtT>
            int captureStartCol(const SubprogramT &subprogram) {
                const auto &startStmt = std::get<Fortran::parser::Statement<StartStmtT> >(subprogram.t);
                if (const auto pos = locationFromSource(parsing, startStmt.source, false); pos.has_value()) {
                    return pos.value().column;
                }
                return 1;
            }

            // MainProgram special-case: ProgramStmt is optional (Fortran
            // permits an implicit main program with no `program` keyword).
            // Falls back to column 1 for the implicit form.
            int captureMainProgramStartCol(const Fortran::parser::MainProgram &mainProgram) {
                const auto &maybeProgStmt =
                    std::get<std::optional<Fortran::parser::Statement<Fortran::parser::ProgramStmt> > >(
                        mainProgram.t);
                if (maybeProgStmt.has_value()) {
                    if (const auto pos = locationFromSource(parsing, maybeProgStmt.value().source, false);
                        pos.has_value()) {
                        return pos.value().column;
                    }
                }
                return 1;
            }

            bool Pre(const Fortran::parser::MainProgram &mainProgram) {
                isInMainProgram_ = true;
                captureBodyEndLines<Fortran::parser::MainProgram, Fortran::parser::EndProgramStmt>(
                    mainProgram, mainProgramEndLine_, mainProgramEndCol_, mainProgramContainsLine_,
                    mainProgramNameEndLine_, mainProgramEndStmt_);
                mainProgramStartCol_ = captureMainProgramStartCol(mainProgram);
                return true;
            }

            bool Pre(const Fortran::parser::SubroutineSubprogram &subprogram) {
                captureBodyEndLines<Fortran::parser::SubroutineSubprogram, Fortran::parser::EndSubroutineStmt>(
                    subprogram, subProgramEndLine_, subProgramEndCol_, subProgramContainsLine_,
                    subProgramNameEndLine_, subProgramEndStmt_);
                subProgramStartCol_ =
                    captureStartCol<Fortran::parser::SubroutineSubprogram, Fortran::parser::SubroutineStmt>(
                        subprogram);
                return true;
            }

            bool Pre(const Fortran::parser::FunctionSubprogram &subprogram) {
                captureBodyEndLines<Fortran::parser::FunctionSubprogram, Fortran::parser::EndFunctionStmt>(
                    subprogram, subProgramEndLine_, subProgramEndCol_, subProgramContainsLine_,
                    subProgramNameEndLine_, subProgramEndStmt_);
                subProgramStartCol_ =
                    captureStartCol<Fortran::parser::FunctionSubprogram, Fortran::parser::FunctionStmt>(subprogram);
                return true;
            }

            // F2018 R1538: separate-module-subprogram, used by the
            // `module procedure foo ... end procedure foo` form in
            // submodules.  Structurally identical to SubroutineSubprogram /
            // FunctionSubprogram (MpSubprogramStmt header, SpecificationPart,
            // ExecutionPart, optional InternalSubprogramPart, EndMpSubprogramStmt
            // tail), and reuses the same body-end bookkeeping.
            bool Pre(const Fortran::parser::SeparateModuleSubprogram &subprogram) {
                captureBodyEndLines<
                    Fortran::parser::SeparateModuleSubprogram,
                    Fortran::parser::EndMpSubprogramStmt>(
                    subprogram, subProgramEndLine_, subProgramEndCol_, subProgramContainsLine_,
                    subProgramNameEndLine_, subProgramEndStmt_);
                subProgramStartCol_ =
                    captureStartCol<Fortran::parser::SeparateModuleSubprogram,
                        Fortran::parser::MpSubprogramStmt>(subprogram);
                return true;
            }

            void Post(const Fortran::parser::MainProgram &) {
                verboseStream() << "Exit main program: " << mainProgramName_ << "\n";
                isInMainProgram_ = false;
                mainProgramEndLine_ = 0;
                mainProgramEndCol_ = 1;
                mainProgramStartCol_ = 1;
                mainProgramContainsLine_ = 0;
                mainProgramNameEndLine_ = 0;
            }

            // Capture the line and file of a procedure's name as `#line`
            // directives give them, as compiler messages do, for the
            // timer name.
            void captureNameStart(const Fortran::parser::CharBlock &source, int &lineOut,
                                  std::string &pathOut) const {
                const auto pos{parsing->allCooked().GetSourcePositionRange(source)->first};
                lineOut = pos.line;
                pathOut = writtenPath(pos);
            }

            void Post(const Fortran::parser::ProgramStmt &program) {
                mainProgramName_ = program.v.ToString();
                captureNameStart(program.v.source, mainProgramLine_, mainProgramPath_);
                verboseStream() << "Enter main program: " << mainProgramName_ << "\n";
            }

            // Returns true when the prefix list marks the subprogram as
            // pure or elemental.  Includes IMPURE ELEMENTAL: the per-element
            // dispatch that elemental procedures see when applied to array
            // arguments would otherwise spawn one TAU timer call per element,
            // drowning real signal.  Issue #36 tracks the design for actually
            // instrumenting these.
            //
            // Separate-module-procedure bodies (F2018 R1538 `module
            // procedure foo ... end procedure foo`) carry no prefix list
            // of their own; pure/elemental attributes live on the
            // matching declaration in the parent module's interface.
            // Those bodies are handled in Pre(MpSubprogramStmt) via the
            // resolved Symbol (attrs are inherited from the interface
            // during semantics, including across .mod-file boundaries).
            static bool prefixSkipsInstrumentation(const std::list<Fortran::parser::PrefixSpec> &prefixes) {
                for (const auto &p : prefixes) {
                    if (std::holds_alternative<Fortran::parser::PrefixSpec::Pure>(p.u) ||
                        std::holds_alternative<Fortran::parser::PrefixSpec::Elemental>(p.u)) {
                        return true;
                    }
                }
                return false;
            }

            // Returns true when the resolved Symbol is pure or elemental.
            // Used for separate-module-procedure bodies whose prefix lives
            // on the parent module's interface declaration.  Flang's
            // semantic phase OR's the interface's attrs into the body
            // symbol's attrs (resolve-names.cpp BeginMpSubprogram), so
            // these helpers see PURE/ELEMENTAL even when the interface
            // lives in a separately-compiled .mod file.  Mirrors the
            // IMPURE ELEMENTAL handling of prefixSkipsInstrumentation.
            static bool symbolSkipsInstrumentation(const Fortran::semantics::Symbol *symbol) {
                if (!symbol) {
                    return false;
                }
                return Fortran::semantics::IsPureProcedure(*symbol) ||
                       Fortran::semantics::IsElementalProcedure(*symbol);
            }

            // Emit a non-suppressible note that a pure or elemental
            // subprogram is being skipped.  Standard-conforming TAU
            // instrumentation cannot be injected into a pure procedure
            // because the macros declare `integer, save :: tauProfileTimer(2)`,
            // and SAVE is forbidden inside a pure procedure (F2018 C1599).
            // Elemental procedures are skipped to avoid per-element timer
            // overhead.
            void notePureOrElementalSkip(const std::string &name, const Fortran::parser::CharBlock &source) const {
                const auto pos = locationFromSource(parsing, source, false);
                const std::string filePath = pos ? writtenPath(*pos) : std::string{"<unknown>"};
                const int line = pos ? pos->line : 0;
                llvm::errs() << "[SALT] " << filePath << ":" << line << ": note: "
                        << "skipping instrumentation of pure or elemental procedure '" << name
                        << "'. Track issue #36.\n";
            }

            bool Pre(const Fortran::parser::SubroutineStmt &subroutineStmt) {
                isInMainProgram_ = false;
                const auto &name = std::get<Fortran::parser::Name>(subroutineStmt.t);
                subprogramName_ = name.ToString();
                captureNameStart(name.source, subProgramLine_, subProgramPath_);
                verboseStream() << "Enter Subroutine: " << subprogramName_ << "\n";
                if (prefixSkipsInstrumentation(std::get<std::list<Fortran::parser::PrefixSpec> >(subroutineStmt.t))) {
                    notePureOrElementalSkip(subprogramName_, name.source);
                    skipInstrumentSubprogram_ = true;
                } else if (!shouldInstrumentSubprogram(subprogramName_)) {
                    verboseStream() << "Skipping instrumentation of " << subprogramName_ <<
                            " due to selective instrumentation\n";
                    skipInstrumentSubprogram_ = true;
                }
                return true;
            }

            void Post(const Fortran::parser::SubroutineSubprogram &) {
                verboseStream() << "Exit Subroutine: " << subprogramName_ << "\n";
                skipInstrumentSubprogram_ = false;
                subprogramName_.clear();
                subProgramEndLine_ = 0;
                subProgramEndCol_ = 1;
                subProgramStartCol_ = 1;
                subProgramContainsLine_ = 0;
                subProgramNameEndLine_ = 0;
            }

            bool Pre(const Fortran::parser::FunctionStmt &functionStmt) {
                isInMainProgram_ = false;
                const auto &name = std::get<Fortran::parser::Name>(functionStmt.t);
                subprogramName_ = name.ToString();
                captureNameStart(name.source, subProgramLine_, subProgramPath_);
                verboseStream() << "Enter Function: " << subprogramName_ << "\n";
                if (prefixSkipsInstrumentation(std::get<std::list<Fortran::parser::PrefixSpec> >(functionStmt.t))) {
                    notePureOrElementalSkip(subprogramName_, name.source);
                    skipInstrumentSubprogram_ = true;
                } else if (!shouldInstrumentSubprogram(subprogramName_)) {
                    verboseStream() << "Skipping instrumentation of " << subprogramName_ <<
                            " due to selective instrumentation\n";
                    skipInstrumentSubprogram_ = true;
                }
                return true;
            }

            void Post(const Fortran::parser::FunctionSubprogram &) {
                verboseStream() << "Exit Function: " << subprogramName_ << "\n";
                skipInstrumentSubprogram_ = false;
                subprogramName_.clear();
                subProgramLine_ = 0;
                subProgramEndLine_ = 0;
                subProgramEndCol_ = 1;
                subProgramStartCol_ = 1;
                subProgramContainsLine_ = 0;
                subProgramNameEndLine_ = 0;
            }

            // Header statement for a separate module procedure body
            // (`module procedure foo` form, F2018 R1539).  Mirrors the
            // SubroutineStmt / FunctionStmt handlers: capture the name and
            // the line of the procedure declaration, honour selective
            // instrumentation, and skip pure/elemental bodies whose
            // attributes are declared on the parent module's interface.
            bool Pre(const Fortran::parser::MpSubprogramStmt &mpStmt) {
                isInMainProgram_ = false;
                subprogramName_ = mpStmt.v.ToString();
                captureNameStart(mpStmt.v.source, subProgramLine_, subProgramPath_);
                verboseStream() << "Enter Module Procedure: " << subprogramName_ << "\n";
                if (symbolSkipsInstrumentation(mpStmt.v.symbol)) {
                    notePureOrElementalSkip(subprogramName_, mpStmt.v.source);
                    skipInstrumentSubprogram_ = true;
                } else if (!shouldInstrumentSubprogram(subprogramName_)) {
                    verboseStream() << "Skipping instrumentation of " << subprogramName_ <<
                            " due to selective instrumentation\n";
                    skipInstrumentSubprogram_ = true;
                }
                return true;
            }

            void Post(const Fortran::parser::SeparateModuleSubprogram &) {
                verboseStream() << "Exit Module Procedure: " << subprogramName_ << "\n";
                skipInstrumentSubprogram_ = false;
                subprogramName_.clear();
                subProgramLine_ = 0;
                subProgramEndLine_ = 0;
                subProgramEndCol_ = 1;
                subProgramStartCol_ = 1;
                subProgramContainsLine_ = 0;
                subProgramNameEndLine_ = 0;
            }

            // Split handling of ExecutionPart into two phases
            // so that we insert Instrumentation Points in order
            // even if we separately insert them in visitors for
            // children of ExecutionPart.
            bool Pre(const Fortran::parser::ExecutionPart &executionPart) {
                handleExecutionPart(executionPart, true);
                return true;
            }

            void Post(const Fortran::parser::ExecutionPart &executionPart) {
                handleExecutionPart(executionPart, false);
            }

            void handleExecutionPart(const Fortran::parser::ExecutionPart &executionPart, bool pre) {
                const Fortran::parser::Block &block = executionPart.v;
                if (block.empty()) {
                    verboseStream() << "WARNING: Execution part empty.\n";
                    return;
                }

                const std::optional startLocOpt{getLocation(parsing, block.front(), false)};
                if (!startLocOpt.has_value()) {
                    llvm::errs() << "ERROR: execution part had no start source location!\n";
                    return;
                }
                const auto &startLoc{startLocOpt.value()};

                // Compute the line that ends the procedure's executable
                // section from the wrapping subprogram/main-program tuple --
                // either the line just before its `contains` keyword (if any)
                // or just before its End statement.  Computing it this way
                // (instead of from `block.back()` which can fail to map back
                // through CookedSource for some statements) handles issue #31
                // and llvm-project#196291 by construction.
                const int containsLine = isInMainProgram_ ? mainProgramContainsLine_ : subProgramContainsLine_;
                const int endStmtLine = isInMainProgram_ ? mainProgramEndLine_ : subProgramEndLine_;
                const int boundaryLine = containsLine > 0 ? containsLine : endStmtLine;
                if (boundaryLine <= 0) {
                    llvm::errs() << "ERROR: could not determine procedure body end; "
                            "neither contains nor End statement source resolved.\n";
                    return;
                }
                const int endLine = boundaryLine - 1;

                // Insert the timer start in the Pre phase (when we first visit the node)
                // and the timer stop in the Post phase (when we return after visiting the node's children).
                if (pre) {
                    // Source-range columns for the TAU timer name follow the
                    // PDT convention (head-begin -> body-end, both inclusive).
                    // Start col is the keyword column captured from the
                    // wrapping Statement<StartStmt>; end col is the last char
                    // of the End statement.  When the body is bounded by a
                    // ContainsStmt instead of the End, fall back to col 1
                    // for the end (the boundary line lands on the
                    // ContainsStmt itself, whose extent we don't track).
                    const int startCol = isInMainProgram_ ? mainProgramStartCol_ : subProgramStartCol_;
                    const int endCol = containsLine > 0
                        ? 1
                        : (isInMainProgram_ ? mainProgramEndCol_ : subProgramEndCol_);
                    // Implicit main programs (no program-stmt; F2018 R1101
                    // makes program-stmt optional) have no source line or
                    // name to anchor the timer to; Post(ProgramStmt) never
                    // fires and mainProgramName_/mainProgramLine_ stay
                    // default-initialised.  Synthesize a fallback name
                    // combining the input file's basename with a fixed
                    // `_implicit_main_` marker (won't collide with any
                    // user symbol since `-` is not a valid Fortran ident
                    // character), and use the first-executable-stmt line.
                    //
                    // The file and lines of the name are those that `#line`
                    // directives give, as in compiler messages.
                    std::string procName;
                    std::string procPath;
                    int procStartLine;
                    if (isInMainProgram_ && mainProgramName_.empty()) {
                        procPath = writtenPath(startLoc);
                        procName = std::filesystem::path(procPath).stem().string() + "._implicit_main_";
                        procStartLine = startLoc.line;
                    } else {
                        procName = isInMainProgram_ ? mainProgramName_ : subprogramName_;
                        procPath = isInMainProgram_ ? mainProgramPath_ : subProgramPath_;
                        procStartLine = isInMainProgram_ ? mainProgramLine_ : subProgramLine_;
                    }
                    std::stringstream ss;
                    ss << procName;
                    ss << " [{" << procPath << "} {";
                    ss << procStartLine;
                    ss << "," << startCol << "}-{";
                    ss << (isInMainProgram_ ? mainProgramNameEndLine_ : subProgramNameEndLine_);
                    ss << "," << endCol << "}]";

                    const std::string timerName{ss.str()};

                    // Split the timerName string so that it will fit between Fortran 77's 72-character limit,
                    // and use character string line continuation syntax compatible with Fortran 77 and modern
                    // Fortran. Each `"` is doubled for the `"`-delimited literal, and a piece never ends
                    // between the two, which would close the literal; a `\` stays as written.
                    std::stringstream ss2;
                    std::string piece;
                    for (const char c : timerName) {
                        const std::string text = c == '"' ? "\"\"" : std::string(1, c);
                        if (piece.size() + text.size() > SALT_F77_LINE_LENGTH) {
                            ss2 << SALT_FORTRAN_STRING_SPLITTER << piece;
                            piece.clear();
                        }
                        piece += text;
                    }
                    if (!piece.empty()) {
                        ss2 << SALT_FORTRAN_STRING_SPLITTER << piece;
                    }

                    const std::string splitTimerName{ss2.str()};

                    if (isInMainProgram_ && mainSetup_) {
                        warnIfMainNotIncluded(procName, /*ignoreCase=*/true);
                    }
                    const bool excludedMain{
                        isInMainProgram_ && (skipInstrumentFile_ || !shouldInstrumentSubprogram(procName))
                    };
                    if ((excludedMain ? mainSetup_ : shouldInstrument()) && followsStatement(block.front())) {
                        // The timer declarations and start, or the setup code, would go before
                        // the line, so before the statement before them on the line. The flag
                        // also drops the timer stops. The name, of up to 63 characters, goes
                        // on a comment line of its own to fit in column 72.
                        llvm::errs() << "[SALT] " << writtenPath(startLoc) << ":" << startLoc.line
                                << ": warning: not instrumenting procedure '" << procName
                                << "': its first executable statement shares the line with the "
                                << "procedure statement or a declaration\n";
                        instrumentationPoints_.emplace_back(std::make_unique<ProcedureSkippedInstrumentationPoint>(
                            startLoc.trueLineNumber, "! SALT-FM could not split line " +
                                                     std::to_string(startLoc.line) + " to instrument\n! " +
                                                     procName));
                        skipInstrumentSubprogram_ = true;
                        lineNotSplit_ = true;
                    } else if (excludedMain) {
                        // Keep the setup code, if any, but not the timer; the
                        // flag also drops the timer stop
                        verboseStream() << "Skipping the timer of " << procName <<
                                " due to selective instrumentation\n";
                        skipInstrumentSubprogram_ = true;
                        if (mainSetup_) {
                            addProgramSetupInstrumentation(startLoc.trueLineNumber, splitTimerName);
                        }
                    } else if (isInMainProgram_) {
                        verboseStream() << "Program begin \"" << mainProgramName_ << "\" at " <<
                                startLoc.trueLineNumber << ", " << startLoc.column << "\n";
                        addProgramBeginInstrumentation(startLoc.trueLineNumber, splitTimerName);
                    } else {
                        verboseStream() << "Subprogram begin \"" << subprogramName_ << "\" at " <<
                                startLoc.trueLineNumber << ", " << startLoc.column << "\n";
                        addProcedureBeginInstrumentation(startLoc.trueLineNumber, splitTimerName);
                    }
                } else {
                    if (const auto &endStmt{isInMainProgram_ ? mainProgramEndStmt_ : subProgramEndStmt_};
                        endStmt.has_value()) {
                        verboseStream() << "End statement at line " << endStmt->start.line << "\n";
                        addProcedureEndInstrumentation(*endStmt);
                    } else {
                        verboseStream() << "End at line " << endLine << "\n";
                        addProcedureEndInstrumentation(endLine);
                    }
                    if (isInMainProgram_ || lineNotSplit_) {
                        // Its internal subprograms choose for themselves
                        skipInstrumentSubprogram_ = false;
                        lineNotSplit_ = false;
                    }
                }
            }

            // Whether a statement comes before `stmt` on its line: the cooked
            // text, which joins continuation lines, has a `;` before it, and
            // text other than `;` and blanks before that on its line. The
            // source of a construct can start after a directive sentinel, such
            // as `!$omp`, on its line.
            template<typename A>
            bool followsStatement(const A &stmt) const {
                const std::optional<Fortran::parser::CharBlock> source{Fortran::parser::GetSource(stmt)};
                if (!source.has_value()) {
                    return false;
                }
                const Fortran::parser::CharBlock cooked{DEREF(parsing->allCooked().Find(*source)).AsCharBlock()};
                const char *before{source->begin()};
                bool semicolon{false};
                while (before > cooked.begin() &&
                       (before[-1] == ' ' || before[-1] == '\t' || before[-1] == ';')) {
                    semicolon |= before[-1] == ';';
                    --before;
                }
                return semicolon && before > cooked.begin() && before[-1] != '\n';
            }

            // A ReturnStmt does not have a source, so we instead need to get access to the wrapper Statement that does.
            // Here we get the ReturnStmt through ExecutableConstruct -> Statement<ActionStmt> -> Indirection<ReturnStmt>
            //
            // The same wrapper Statement<ActionStmt> covers an `if (<cond>) return` IfStmt.  Detecting the
            // if-return form here (rather than in Pre(IfStmt&)) gives us access to the wrapper's source
            // CharBlock, which spans the full physical extent of the if-stmt -- including any `&` line
            // continuations inside the condition or between the condition and the action.
            //
            // inputPosition() DIEs when it cannot map a CharBlock.
            // Possible causes of an unmapped CharBlock include
            // non-conforming Fortran (e.g. a deleted construct -- see
            // F2018 Annex B.2.2 / R813 / R823 for the labeled-DO
            // terminator rules), upstream flang source-mapping bugs (cf.
            // llvm-project#196291), or other situations that haven't been
            // characterized yet.
            bool Pre(const Fortran::parser::ExecutableConstruct &execConstruct) {
                if (const auto actionStmt = std::get_if<Fortran::parser::Statement<Fortran::parser::ActionStmt> >(
                    &execConstruct.u)) {
                    if (const auto retInd =
                            std::get_if<Fortran::common::Indirection<Fortran::parser::ReturnStmt> >(
                                &actionStmt->statement.u)) {
                        // Both plain `return` and the obsolescent alternate-return
                        // form `return <scalar-int-expr>` are handled identically:
                        // we inject TAU_PROFILE_STOP directly before the statement,
                        // and the user's `return [<expr>]` is echoed unchanged.  Issue #32.
                        if (retInd->value().v.has_value()) {
                            noteAlternateReturn(actionStmt->source, /*insideIfStmt=*/false);
                        }
                        const StatementPositions pos{statementPositions(*actionStmt)};
                        verboseStream() << "Return statement at " << pos.start.line << "\n";
                        addReturnStmtInstrumentation(pos);
                    } else if (const auto ifInd = std::get_if<Fortran::common::Indirection<Fortran::parser::IfStmt> >(
                        &actionStmt->statement.u)) {
                        const Fortran::parser::IfStmt &ifStmt{ifInd->value()};
                        const auto &innerAction{
                            std::get<Fortran::parser::UnlabeledStatement<Fortran::parser::ActionStmt> >(ifStmt.t)
                        };
                        if (const auto innerRet =
                                std::get_if<Fortran::common::Indirection<Fortran::parser::ReturnStmt> >(
                                    &innerAction.statement.u)) {
                            // The wrapping Statement<ActionStmt>'s source covers the entire `if (...) return`
                            // including continuations.  Use it to compute the physical range to replace, and
                            // the source of the inner `return [<expr>]` for the part that `then` replaces.
                            const StatementPositions pos{statementPositions(*actionStmt)};
                            if (innerRet->value().v.has_value()) {
                                noteAlternateReturn(actionStmt->source, /*insideIfStmt=*/true);
                            }
                            verboseStream() << "If-return spans lines " << pos.start.line << "-"
                                    << pos.end.line
                                    << (actionStmt->label.has_value()
                                            ? ", label: " + std::to_string(actionStmt->label.value())
                                            : "")
                                    << "\n";
                            // The last letter of the `return` keyword, after blanks in fixed form
                            const Fortran::parser::CharBlock &returnSource{innerAction.source};
                            const char *last{returnSource.begin()};
                            for (int letters{1}; letters < 6 && last + 1 < returnSource.end();) {
                                if (*++last != ' ') {
                                    ++letters;
                                }
                            }
                            const InputPosition returnStart{inputPosition(returnSource, false)};
                            addIfReturnStmtInstrumentation(pos, returnStart,
                                                           inputPosition({last, 1}, false).line != returnStart.line);
                        }
                    }
                }
                return true;
            }

            // Informational note that the user's source uses the obsolescent
            // alternate-return form (`return <scalar-int-expr>`, optionally
            // inside `if (cond)`).  Listed under Fortran 2018 Annex B.3
            // "Obsolescent features"; future revisions of the standard may
            // delete it.  SALT instruments these statements correctly today
            // (issue #32 closed); the note exists so users know to migrate
            // away from the form, not because instrumentation skipped it.
            //
            // Emitted to stderr at always-on severity (not gated on verbose)
            // so it is visible during a normal build.
            void noteAlternateReturn(const Fortran::parser::CharBlock &source, const bool insideIfStmt) const {
                const auto pos = locationFromSource(parsing, source, false);
                const std::string filePath = pos ? writtenPath(*pos) : std::string{"<unknown>"};
                const int line = pos ? pos->line : 0;
                llvm::errs() << "[SALT] " << filePath << ":" << line << ": note: "
                        << (insideIfStmt
                                ? "instrumenting `if (<cond>) return <expr>`, "
                                : "instrumenting `return <expr>`, ")
                        << "an obsolescent alternate-return form "
                        << "(Fortran 2018 Annex B.3); consider replacing it with structured control flow.\n";
            }

        private:
            // Keeps track of current state of traversal
            bool isInMainProgram_{false};
            std::string mainProgramName_;
            // The line and file of the name, and the line of the body's
            // end, for the timer name, as `#line` directives give them
            int mainProgramLine_{0};
            std::string mainProgramPath_;
            int mainProgramNameEndLine_{0};
            // Physical line numbers of the wrapping End{Program,Subroutine,Function}Stmt
            // and (when present) the ContainsStmt that begins the
            // InternalSubprogramPart.  Used to bound the procedure's
            // executable section from above instead of relying on the last
            // ExecutionPartConstruct's source mapping (issue #31).
            int mainProgramEndLine_{0};
            int mainProgramContainsLine_{0};
            // Without a `contains`, the input positions of the End
            // statement, where the timer stop goes
            std::optional<StatementPositions> mainProgramEndStmt_;
            // Columns for the TAU timer name source-range field.  Captured
            // from the wrapping Statement<StartStmt>::source for start
            // (keyword position; defaults to 1 for the implicit-main form
            // and as a generic fallback) and from
            // Statement<EndStmt>::source's last-character column for end
            // (PDT-style inclusive last-char convention).
            int mainProgramStartCol_{1};
            int mainProgramEndCol_{1};
            std::string subprogramName_;
            int subProgramLine_{0};
            std::string subProgramPath_;
            int subProgramNameEndLine_{0};
            int subProgramEndLine_{0};
            int subProgramContainsLine_{0};
            std::optional<StatementPositions> subProgramEndStmt_;
            int subProgramStartCol_{1};
            int subProgramEndCol_{1};

            bool skipInstrumentFile_;
            bool skipInstrumentSubprogram_{false};
            // Whether the procedure has no timers as a line cannot be split
            bool lineNotSplit_{false};
            // Whether a main program that the lists leave out gets its setup code
            bool mainSetup_;

            std::vector<std::unique_ptr<const InstrumentationPoint> > instrumentationPoints_;

            // Pass in the parser object from the Action to the Visitor
            // so that we can use it while processing parse tree nodes.
            Fortran::parser::Parsing *parsing{nullptr};
        }; // SaltInstrumentParseTreeVisitor

        /**
         * Get the source file represented by a given parse tree
         *
         * See function BuildRuntimeDerivedTypeTables() in
         * flang/lib/Semantics/runtime-type-info.cpp for example
         * of getting the source file name.
         */
        [[nodiscard]] static const Fortran::parser::SourceFile *getInputFile(Fortran::parser::Parsing &parsing) {
            const auto &allSources{parsing.allCooked().allSources()};
            if (const auto firstProv{allSources.GetFirstFileProvenance()}) {
                return allSources.GetSourceFile(firstProv->start());
            }
            return nullptr;
        }

        // Returns the `#line` directive to insert in the output before physical line
        // `line` of `file`. A physical line is a line's position in `file`. Its
        // computed line is the line number, and maybe the file, that the `#line`
        // directives above it give it, as compiler messages report it.
        // The returned `#line` directive gives the computed line number of `line`.
        // It also names `file` when the computed file of `line` is `file`.
        // Otherwise it gives only the number, so the compiler keeps the file of
        // the last `#line` directive that it read with a file name: one of the
        // input's, as the output keeps all of them. SALT-FM does not write that
        // file name itself, as Flang reports it with each `\` doubled.
        // `line` may be one past the last physical line, after an insert on the
        // last line; its computed line number is then that of the last physical
        // line plus 1.
        static std::string lineDirective(const Fortran::parser::SourceFile &file, const int line) {
            const int last{static_cast<int>(file.lines())};
            if (last == 0) {
                return "#line " + std::to_string(line) + " \"" + file.path() + "\"";
            }
            const int at{std::min(line, last)};
            const Fortran::parser::SourcePosition pos{file.GetSourcePosition(file.GetLineStartOffset(at))};
            const std::string directive{"#line " + std::to_string(pos.line + line - at)};
            return *pos.path == file.path() ? directive + " \"" + file.path() + "\"" : directive;
        }

        // `text` with each character before index `from` made a blank, but a tab,
        // so the rest keeps its columns, also in a fixed-form line that has a tab
        static std::string blankBefore(std::string text, const std::size_t from) {
            for (std::size_t i{0}; i < std::min(from, text.size()); ++i) {
                if (text[i] != '\t') {
                    text[i] = ' ';
                }
            }
            return text;
        }

        static void instrumentFile(const Fortran::parser::SourceFile &inputFile, std::ifstream &inputStream,
                                   llvm::raw_pwrite_stream &outputStream,
                                   const SaltInstrumentParseTreeVisitor &visitor,
                                   const InstrumentationMap &instMap, const bool fixedForm) {
            std::string lineText;
            int lineNum{0};
            const auto &instPts{visitor.getInstrumentationPoints()};

            verboseStream() << "Will perform instrumentation:\n" << visitor.dumpInstrumentationPoints();

            outputStream << lineDirective(inputFile, 1) << "\n";

            // Reads the input through line `last`, the continuation of a Fortran line
            // up to the statement after a return statement, or up to the text of a
            // statement after its label. Keeps the directives, such as a `#line`
            // directive, except an `#include` directive (in any case, as for Flang),
            // whose text is part of the line.
            // As for Flang, a directive starts with a `#` after blanks, except that in
            // fixed form a `#` in column 6 after spaces marks a continuation line.
            // Each kept directive gets a `#line` directive with its own computed line,
            // and the lines that continue a directive after a `\` follow it.
            const auto skipThrough = [&](const int last) {
                bool continuedDirective{false};
                while (lineNum < last && std::getline(inputStream, lineText)) {
                    ++lineNum;
                    if (llvm::StringRef directive{llvm::StringRef{lineText}.ltrim(" \t")};
                        continuedDirective ||
                        (!(fixedForm && llvm::StringRef{lineText}.starts_with("     #")) &&
                         directive.consume_front("#") &&
                         !directive.ltrim().starts_with_insensitive("include"))) {
                        if (!continuedDirective) {
                            outputStream << lineDirective(inputFile, lineNum) << "\n";
                        }
                        outputStream << lineText << "\n";
                        continuedDirective = llvm::StringRef{lineText}.rtrim("\r").ends_with("\\");
                    }
                }
            };

            // Writes the input from index `from` of the current line to just before
            // `to`, without the final newline. The text before `from` is made blank,
            // and the lines between, such as comment lines and directives, stay as
            // written.
            const auto writeThrough = [&](std::size_t from, const InputPosition to) {
                outputStream << lineDirective(inputFile, lineNum) << "\n";
                while (lineNum < to.line) {
                    outputStream << blankBefore(lineText, from) << "\n";
                    from = 0;
                    if (!std::getline(inputStream, lineText)) {
                        return;
                    }
                    ++lineNum;
                }
                outputStream << blankBefore(lineText.substr(0, to.column - 1), from);
            };

            auto instIter{instPts.cbegin()};
            while (std::getline(inputStream, lineText)) {
                bool lineWasInstrumentedBefore{false};
                bool lineWasInstrumentedAfter{false};
                bool replaced{false};

                ++lineNum;

                // First, process instrumentation points that come BEFORE this line.
                while (instIter != instPts.cend() && (*instIter)->line() == lineNum && (*instIter)->location() ==
                       InstrumentationLocation::BEFORE) {
                    outputStream << (*instIter)->instrumentationString(instMap, lineText) << "\n";
                    lineWasInstrumentedBefore = true;
                    ++instIter;
                }

                // Then, process the return and End statements that start on this line,
                // which the output replaces with the TAU stop-timer call and their text.
                // They can span lines, as when an if-stmt's condition is split via `&`
                // continuation. The text before them on their lines, such as statements
                // and the `;` after them, and the statements after them go on lines of
                // their own, with the text before them made blank, so they keep their
                // columns and labels; in fixed form, they keep the label field, the
                // continuation column, and the line length. `rest` is the index in
                // lineText of the text that is left to write, if any.
                std::size_t rest{0};
                while (instIter != instPts.cend() && (*instIter)->line() == lineNum && (*instIter)->location() ==
                       InstrumentationLocation::REPLACE) {
                    // SALT links against LLVM with -fno-rtti, so dynamic_cast is
                    // unavailable; only return and End statements replace their text.
                    const auto &pt{static_cast<const ReturnInstrumentationPoint &>(**instIter)};
                    if (const std::string before{blankBefore(lineText.substr(0, pt.start().column - 1), rest)};
                        before.find_first_not_of(" \t") != std::string::npos) {
                        outputStream << lineDirective(inputFile, lineNum) << "\n";
                        outputStream << llvm::StringRef{before}.rtrim(" \t") << "\n";
                    }
                    std::size_t from{static_cast<std::size_t>(pt.start().column - 1)};
                    if (pt.instrumentationType() == InstrumentationPointType::IF_RETURN) {
                        const auto &ifReturn{static_cast<const IfReturnStmtInstrumentationPoint &>(pt)};
                        const InputPosition returnStart{ifReturn.returnStart()};
                        writeThrough(from, returnStart);
                        // `then` goes on a continuation line after the start of a `return`
                        // keyword split over lines, where it could pass the line length
                        if (ifReturn.returnSplit()) {
                            outputStream << (fixedForm ? "\n     &" : "&\n     &");
                        }
                        outputStream << "then\n";
                        outputStream << pt.instrumentationString(instMap, lineText) << "\n";
                        writeThrough(returnStart.column - 1, pt.end());
                        outputStream << "\n      end if\n";
                    } else {
                        outputStream << lineDirective(inputFile, lineNum) << "\n";
                        outputStream << pt.instrumentationString(instMap, lineText) << "\n";
                        // The `continue` before the stop-timer call has the label, so the
                        // statement goes on from its text after the label, whose line becomes
                        // its first line: the lines before go, and the text before it, such as
                        // a continuation mark, is made blank
                        if (const auto &label{pt.label()}; label) {
                            skipThrough(label->text.line);
                            from = label->text.column - 1;
                        }
                        writeThrough(from, pt.end());
                        outputStream << "\n";
                    }
                    if (const auto &next{pt.next()}; next) {
                        skipThrough(next->line);
                        rest = next->column - 1;
                    } else {
                        // A comment after the statement, if any
                        rest = lineText.find('!', pt.end().column - 1);
                    }
                    lineWasInstrumentedAfter = true;
                    replaced = true;
                    ++instIter;
                }

                if (replaced) {
                    // The points on the lines that a statement's Fortran line continued
                    // over, as the end of a procedure whose `contains` follows it
                    while (instIter != instPts.cend() && (*instIter)->line() < lineNum) {
                        outputStream << (*instIter)->instrumentationString(instMap, lineText) << "\n";
                        ++instIter;
                    }
                    if (rest < lineText.size()) {
                        outputStream << lineDirective(inputFile, lineNum) << "\n";
                        outputStream << blankBefore(lineText, rest) << "\n";
                    }
                } else {
                    // If line was instrumented, output a #line directive
                    if (lineWasInstrumentedBefore) {
                        outputStream << lineDirective(inputFile, lineNum) << "\n";
                    }
                    outputStream << lineText << "\n";
                }

                // Finally, process instrumentation points that come AFTER this line.
                while (instIter != instPts.cend() && (*instIter)->line() == lineNum && (*instIter)->location() ==
                       InstrumentationLocation::AFTER) {
                    outputStream << (*instIter)->instrumentationString(instMap, lineText) << "\n";
                    lineWasInstrumentedAfter = true;
                    ++instIter;
                }

                // If line was instrumented, output a #line directive
                if (lineWasInstrumentedAfter) {
                    outputStream << lineDirective(inputFile, lineNum+1) << "\n";
                }
            }
        }

        [[nodiscard]] static std::string getConfigPath() {
            // fparse-llvm always sets the config path env var; the default
            // is only for a Flang run without it
            if (const char *val = getenv(SALT_FORTRAN_CONFIG_FILE_VAR)) {
                return val;
            }
            return SALT_FORTRAN_CONFIG_DEFAULT_PATH;
        }

        [[nodiscard]] static std::optional<std::string> getSelectFilePath() {
            if (const char *val = getenv(SALT_FORTRAN_SELECT_FILE_VAR)) {
                if (std::string selectFile{val}; !selectFile.empty()) {
                    return selectFile;
                }
            }
            return std::nullopt;
        }

        // The value that fparse-llvm gives in the environment; `routine` if none
        [[nodiscard]] static ExcludedMainSetup getExcludedMainSetup() {
            const char *val = getenv(SALT_FORTRAN_EXCLUDED_MAIN_SETUP_VAR);
            const std::string level{val ? val : ""};
            if (level == "none") {
                return ExcludedMainSetup::none;
            }
            return level == "file" ? ExcludedMainSetup::file : ExcludedMainSetup::routine;
        }

        // The code lines of `key` under the "Fortran" node of the configuration
        // file `configPath`, one per line. Exits with an error when the key is
        // missing, unless `optional`, or is not a list of strings; an empty one
        // gives no lines.
        [[nodiscard]] static std::string getCodeLines(const ryml::ConstNodeRef fortranNode, const char *key,
                                                      const std::string &configPath, const bool optional = false) {
            const ryml::ConstNodeRef node{check_config_key(fortranNode, key, configPath, SALT_FORTRAN_KEY, optional)};
            if (node.invalid()) {
                return "";
            }
            std::stringstream ss;
            for (const ryml::ConstNodeRef child: node.children()) {
                ss << child.val() << "\n";
            }
            return ss.str();
        }

        [[nodiscard]] static InstrumentationMap getInstrumentationMap(const std::string &configPath) {
            InstrumentationMap map;
            const ryml::Tree tree{read_config_file(configPath)};

            // Access the "Fortran" node
            const ryml::ConstNodeRef fortranNode{check_config_key(tree.crootref(), SALT_FORTRAN_KEY, configPath)};

            // The optional "program_setup_insert" node: setup code at the start
            // of the main program, inserted with or without its timer.
            const std::string programSetup{getCodeLines(fortranNode, SALT_FORTRAN_PROGRAM_SETUP_KEY, configPath, true)};
            map.emplace(InstrumentationPointType::PROGRAM_SETUP, programSetup);

            // The "program_insert" node: the main program's timer, after its
            // setup code
            map.emplace(InstrumentationPointType::PROGRAM_BEGIN,
                        programSetup + getCodeLines(fortranNode, SALT_FORTRAN_PROGRAM_BEGIN_KEY, configPath));

            map.emplace(InstrumentationPointType::PROCEDURE_BEGIN,
                        getCodeLines(fortranNode, SALT_FORTRAN_PROCEDURE_BEGIN_KEY, configPath));

            const std::string procedureEnd{getCodeLines(fortranNode, SALT_FORTRAN_PROCEDURE_END_KEY, configPath)};
            map.emplace(InstrumentationPointType::PROCEDURE_END, procedureEnd);
            // The return statement uses the same text as procedure end,
            // but is inserted before the line instead of after.
            map.emplace(InstrumentationPointType::RETURN_STMT, procedureEnd);
            // The if-return statement uses the same text as procedure end,
            // but requires transformation to if-then-endif
            map.emplace(InstrumentationPointType::IF_RETURN, procedureEnd);

            return map;
        }

        [[nodiscard]] static bool shouldInstrumentFile(const std::filesystem::path &filePath) {
            return ::instrumentFile(filePath.string());
        }

        static void dumpSelectiveRequests() {
            const auto printStr = [&](const auto &a) { verboseStream() << a << "\n"; };
            verboseStream() << "File include list:\n";
            std::for_each(fileincludelist.cbegin(), fileincludelist.cend(), printStr);
            verboseStream() << "File exclude list:\n";
            std::for_each(fileexcludelist.cbegin(), fileexcludelist.cend(), printStr);
            verboseStream() << "Include list:\n";
            std::for_each(includelist.cbegin(), includelist.cend(), printStr);
            verboseStream() << "Exclude list:\n";
            std::for_each(excludelist.cbegin(), excludelist.cend(), printStr);
        }

#if LLVM_VERSION_MAJOR < 23
        /**
         * Runs the frontend steps before executeAction(). Flang 19-22 put
         * the -fintrinsic-modules-path directories only in the ordinary
         * module search, so `use, intrinsic` cannot find SALT-FM's module
         * copies, and the copies hide user modules of the same name. Add
         * the directories to the intrinsic module directories first, as
         * Flang 23 does.
         */
        bool beginSourceFileAction() override {
            auto &ci{getInstance()};
            auto &invoc{ci.getInvocation()};
            const auto &extra{invoc.getPreprocessorOpts().searchDirectoriesFromIntrModPath};
#if LLVM_VERSION_MAJOR >= 21
            // runSemanticChecks() makes a new context from these options
            auto &dirs{invoc.getFortranOpts().intrinsicModuleDirectories};
            dirs.insert(dirs.begin(), extra.begin(), extra.end());
#else
            // LLVM 19, 20: the context exists before this action runs
            auto &ctx{ci.getSemanticsContext()};
            std::vector<std::string> dirs{extra};
            dirs.insert(dirs.end(), ctx.intrinsicModuleDirectories().begin(),
                        ctx.intrinsicModuleDirectories().end());
            ctx.set_intrinsicModuleDirectories(dirs);
#endif
            // Same steps as PrescanAndSemaAction::beginSourceFileAction (private)
            return runPrescan() && runParse(/*emitMessages=*/false) &&
                   runSemanticChecks() && generateRtTypeTables();
        }
#endif

        /**
         * This is the entry point for the plugin.
         */
        void executeAction() override {
            if (getEnvBool(SALT_FORTRAN_VERBOSE_VAR)) {
                enableVerbose();
            }

            verboseStream() << "==== SALT Instrumentor Plugin starting ====\n";

            // This is the object through which we access the parse tree
            // and the source
            Fortran::parser::Parsing &parsing = getParsing();

            // Get the input file and its path
            const Fortran::parser::SourceFile *inputFile{getInputFile(parsing)};
            if (!inputFile) {
                llvm::errs() << "ERROR: Unable to find input file name!\n";
                std::exit(EXIT_FAILURE);
            }

            verboseStream() << "Have input file: " << inputFile->path() << "\n";

            const std::filesystem::path inputFilePath{inputFile->path()};

            if (const auto selectPath{getSelectFilePath()}; selectPath.has_value()) {
                if (processInstrumentationRequests(selectPath->c_str())) {
                    dumpSelectiveRequests();
                } else {
                    // processInstrumentationRequests() reports a file that it cannot read
                    std::exit(EXIT_FAILURE);
                }
            }

            // If visitor has skipInstrument set, no instrumentation points are added
            // so the file is output into the .inst file unchanged.
            bool skipInstrument{false};
            if (!shouldInstrumentFile(inputFilePath)) {
                verboseStream() << "Skipping instrumentation of " << inputFilePath
                        << " due to selective instrumentation.\n";
                skipInstrument = true;
            }
            // Walk the parse tree -- marks nodes for instrumentation
            const ExcludedMainSetup level{getExcludedMainSetup()};
            SaltInstrumentParseTreeVisitor visitor{&parsing, skipInstrument, level != ExcludedMainSetup::none};
            // Unless --tau_excluded_main_setup=file, fparse-llvm runs Flang on an excluded
            // source only for its module files, so it gets no instrumentation points: a copy.
            if (!skipInstrument || level == ExcludedMainSetup::file) {
                Walk(parsing.parseTree(), visitor);
            }

            // Read and parse the yaml configuration file, before the output is
            // opened, unless the output is a copy, which does not need it
            InstrumentationMap instMap;
            if (!visitor.getInstrumentationPoints().empty()) {
                instMap = getInstrumentationMap(getConfigPath());
            }

            // Get the extension of the input file
            // For input file 'filename.ext' we will output to 'filename.inst.Ext'
            // Since we are adding preprocessor directives in the emitted code,
            // the file extension should be capitalized.
            std::string inputFileExtension;
            if (auto const extPos = inputFilePath.string().find_last_of('.'); extPos == std::string::npos) {
                inputFileExtension = "F90"; // Default if for some reason file has no extension
            } else {
                inputFileExtension = inputFilePath.string().substr(extPos + 1); // Part of string past last '.'
                // Capitalize the first character of inputFileExtension
                if (!inputFileExtension.empty()) {
                    inputFileExtension[0] = static_cast<char>(std::toupper(inputFileExtension[0]));
                }
            }

            // Check the order of the instrumentation points before the output is
            // opened, so points out of line order leave no output
            if (const auto &instPts{visitor.getInstrumentationPoints()};
                !std::is_sorted(instPts.cbegin(), instPts.cend(), [](const auto &p1, const auto &p2) {
                    return *p1 < *p2;
                })) {
                llvm::errs() << "ERROR: Instrumentation points of " << inputFilePath.string()
                        << " are not in line order, as when an included file holds instrumented code\n";
                std::exit(EXIT_FAILURE);
            }

            // Open the input before the output, so an input error leaves no output
            std::ifstream inputStream{inputFilePath};
            if (!inputStream) {
                llvm::errs() << "ERROR: Could not open input file " << inputFilePath.string() << "\n";
                std::exit(EXIT_FAILURE);
            }

            // Open an output file for writing the instrumented code: the -o file,
            // else, as Flang's createOutputFile names it, the input with this extension
            const std::string outputFileExtension = "inst."s + inputFileExtension;
            std::string outputPath{getInstance().getFrontendOpts().outputFile};
            if (outputPath.empty()) {
                llvm::SmallString<128> path{getCurrentFileOrBufferName()};
                llvm::sys::path::replace_extension(path, outputFileExtension);
                outputPath = std::string{path};
            }
            // raw_fd_ostream writes `-` to standard output, as Flang does
            const std::optional<std::string> writePath{outputPath == "-" ? outputPath
                                                                          : openOutput(outputPath)};
            if (!writePath) {
                std::exit(EXIT_FAILURE);
            }
            std::error_code openError;
            llvm::raw_fd_ostream outputStream{*writePath, openError};
            if (openError) {
                llvm::errs() << "ERROR: Could not open output file " << outputPath << ": "
                        << openError.message() << "\n";
                std::exit(EXIT_FAILURE);
            }

            // Use the instrumentation points stored in the Visitor to write the instrumented file.
            // The source form, as Flang's prescan takes it
            const FortranForm form{getInstance().getFrontendOpts().fortranForm};
            const bool fixedForm{form == FortranForm::Unknown ? getCurrentInput().getIsFixedForm()
                                                              : form == FortranForm::FixedForm};
            instrumentFile(*inputFile, inputStream, outputStream, visitor, instMap, fixedForm);

            outputStream.close();
            if (outputStream.has_error()) {
                llvm::errs() << "ERROR: Could not write output file " << outputPath << ": "
                        << outputStream.error().message() << "\n";
                outputStream.clear_error();
                std::exit(EXIT_FAILURE);
            }
            if (!keepOutputs()) {
                std::exit(EXIT_FAILURE);
            }

            verboseStream() << "==== SALT Instrumentor Plugin finished ====\n";
        }
    };
}

[[maybe_unused]] static FrontendPluginRegistry::Add<salt::fortran::SaltInstrumentAction> X(
    "salt-instrument", "Apply SALT Instrumentation");
