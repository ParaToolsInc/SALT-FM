/* Copyright (C) 2025, ParaTools, Inc.

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

#ifndef FLANG_INSTRUMENTATION_POINT_HPP
#define FLANG_INSTRUMENTATION_POINT_HPP

#include <string>
#include <map>
#include <optional>
#include <utility>

namespace salt::fortran {
    enum class InstrumentationPointType {
        PROGRAM_BEGIN, // Setup code, then start the main program's timer
        PROGRAM_SETUP, // Setup code, for a main program without a timer
        PROCEDURE_BEGIN, // Declare profiler, start timer
        PROCEDURE_END, // Stop timer on the line after, or directly before the End statement
        RETURN_STMT, // Stop timer directly before the return
        IF_RETURN, // Transform if to if-then-endif, stop timer before return
        PROCEDURE_SKIPPED // A comment that SALT-FM could not instrument the procedure
    };

    // A place in the input: a physical line and a column, which counts
    // bytes from 1
    struct InputPosition {
        int line;
        int column;
    };

    // The label of a statement, and where the statement goes on after it,
    // maybe on a continuation line
    struct StatementLabel {
        long number;
        InputPosition text;
    };

    enum class InstrumentationLocation {
        BEFORE,
        AFTER,
        REPLACE
    };

    using InstrumentationMap = std::map<InstrumentationPointType, const std::string>;

    class InstrumentationPoint {
    public:
        InstrumentationPoint(const InstrumentationPointType type, const int line,
                             const InstrumentationLocation location) : instrumentationType_(type), line_(line),
                                                                       location_(location) {
        }

        virtual ~InstrumentationPoint() = default;

        [[nodiscard]] InstrumentationPointType instrumentationType() const {
            return instrumentationType_;
        }

        [[nodiscard]] int line() const {
            return line_;
        }

        [[nodiscard]] InstrumentationLocation location() const {
            return location_;
        }

        [[nodiscard]] bool instrumentBefore() const {
            return location() == InstrumentationLocation::BEFORE;
        }

        bool operator<(const InstrumentationPoint &other) const {
            if (line() == other.line()) {
                if (instrumentBefore() && !other.instrumentBefore()) {
                    return true;
                }
                return false;
            }
            return line() < other.line();
        }

        [[nodiscard]] std::string typeString() const;

        [[nodiscard]] std::string locationString() const;

        [[nodiscard]] virtual std::string toString() const;

        [[nodiscard]] virtual std::string instrumentationString(const InstrumentationMap &instMap,
                                                                const std::string &lineText) const;

    private:
        const InstrumentationPointType instrumentationType_;
        const int line_;
        const InstrumentationLocation location_;
    };

    // PROGRAM_BEGIN, or PROGRAM_SETUP for a main program without a timer,
    // whose setup code may name the timer too
    class ProgramBeginInstrumentationPoint final : public InstrumentationPoint {
    public:
        ProgramBeginInstrumentationPoint(const int line, std::string timerName,
                                         const InstrumentationPointType type =
                                                 InstrumentationPointType::PROGRAM_BEGIN) : InstrumentationPoint(
                type, line, InstrumentationLocation::BEFORE),
            timerName_(std::move(timerName)) {
        }

        [[nodiscard]] std::string timerName() const {
            return timerName_;
        }

        [[nodiscard]] std::string toString() const override;

        [[nodiscard]] std::string instrumentationString(const InstrumentationMap &instMap,
                                                        const std::string &lineText) const override;

    private:
        const std::string timerName_;
    };

    class ProcedureBeginInstrumentationPoint final : public InstrumentationPoint {
    public:
        ProcedureBeginInstrumentationPoint(const int line, std::string timerName) : InstrumentationPoint(
                InstrumentationPointType::PROCEDURE_BEGIN,
                line,
                InstrumentationLocation::BEFORE),
            timerName_(std::move(timerName)) {
        }

        [[nodiscard]] std::string timerName() const {
            return timerName_;
        }

        [[nodiscard]] std::string toString() const override;

        [[nodiscard]] std::string instrumentationString(const InstrumentationMap &instMap,
                                                        const std::string &lineText) const override;

    private:
        const std::string timerName_;
    };

    // A comment, `comment`, where the timer declarations and start of a
    // procedure would go, that SALT-FM could not instrument the procedure
    class ProcedureSkippedInstrumentationPoint final : public InstrumentationPoint {
    public:
        ProcedureSkippedInstrumentationPoint(const int line, std::string comment) : InstrumentationPoint(
                InstrumentationPointType::PROCEDURE_SKIPPED, line, InstrumentationLocation::BEFORE),
            comment_(std::move(comment)) {
        }

        [[nodiscard]] std::string instrumentationString(const InstrumentationMap &,
                                                        const std::string &) const override {
            return comment_;
        }

    private:
        const std::string comment_;
    };

    class ProcedureEndInstrumentationPoint final : public InstrumentationPoint {
    public:
        explicit ProcedureEndInstrumentationPoint(const int line) : InstrumentationPoint(
            InstrumentationPointType::PROCEDURE_END, line, InstrumentationLocation::AFTER) {
        }
    };

    // A `return` statement, an if-return, or the End statement of a
    // procedure without internal procedures, which replaces its text in
    // the input with the TAU stop-timer call and its text. The statement
    // spans the input from `start` to just before `end`, maybe over
    // continuation lines. `next` is the first character of the statement
    // after it on the same Fortran line, if any, so the output can put
    // that statement on a line of its own. With `label`, the label of the
    // statement, the label goes on a `continue` before the stop-timer
    // call, as labels are local to a procedure, so a branch to the
    // statement also stops the timer.
    class ReturnInstrumentationPoint : public InstrumentationPoint {
    public:
        ReturnInstrumentationPoint(const InstrumentationPointType type, const InputPosition start,
                                   const InputPosition end, const std::optional<InputPosition> next,
                                   const std::optional<StatementLabel> label) : InstrumentationPoint(
                type, start.line, InstrumentationLocation::REPLACE),
            start_(start), end_(end), next_(next), label_(label) {
        }

        [[nodiscard]] InputPosition start() const {
            return start_;
        }

        [[nodiscard]] InputPosition end() const {
            return end_;
        }

        [[nodiscard]] const std::optional<InputPosition> &next() const {
            return next_;
        }

        [[nodiscard]] const std::optional<StatementLabel> &label() const {
            return label_;
        }

        [[nodiscard]] std::string toString() const override;

        [[nodiscard]] std::string instrumentationString(const InstrumentationMap &instMap,
                                                        const std::string &lineText) const override;

    private:
        const InputPosition start_;
        const InputPosition end_;
        const std::optional<InputPosition> next_;
        const std::optional<StatementLabel> label_;
    };

    // A `return` statement: the output keeps its text, after the TAU
    // stop-timer call
    class ReturnStmtInstrumentationPoint final : public ReturnInstrumentationPoint {
    public:
        ReturnStmtInstrumentationPoint(const InputPosition start, const InputPosition end,
                                       const std::optional<InputPosition> next,
                                       const std::optional<StatementLabel> label) : ReturnInstrumentationPoint(
            InstrumentationPointType::RETURN_STMT, start, end, next, label) {
        }
    };

    // An `if (<cond>) return [<expr>]`, maybe over continuation lines,
    // which the output turns into an `if (<cond>) then ... end if` block:
    // the text of the if-statement, with `then` in place of its
    // `return [<expr>]`, which starts at `returnStart`, then the TAU
    // stop-timer call, the text of the `return [<expr>]`, and `end if`.
    // So the condition, with its comments, character literals, label,
    // and continuation lines, stays as written. Its label stays in its
    // text, so the stop-timer call gets none. `returnSplit` tells whether
    // the `return` keyword goes on in a continuation line.
    class IfReturnStmtInstrumentationPoint final : public ReturnInstrumentationPoint {
    public:
        IfReturnStmtInstrumentationPoint(const InputPosition start, const InputPosition end,
                                         const std::optional<InputPosition> next,
                                         const InputPosition returnStart,
                                         const bool returnSplit) : ReturnInstrumentationPoint(
                InstrumentationPointType::IF_RETURN, start, end, next, std::nullopt),
            returnStart_(returnStart), returnSplit_(returnSplit) {
        }

        [[nodiscard]] InputPosition returnStart() const {
            return returnStart_;
        }

        [[nodiscard]] bool returnSplit() const {
            return returnSplit_;
        }

        [[nodiscard]] std::string toString() const override;

    private:
        const InputPosition returnStart_;
        const bool returnSplit_;
    };
}

#endif //FLANG_INSTRUMENTATION_POINT_HPP
