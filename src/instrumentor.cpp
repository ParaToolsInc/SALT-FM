#define RYML_SINGLE_HDR_DEFINE_NOW
#define RYML_SHARED

#include <sstream>

#include <ryml_all.hpp>
#include "instrumentor.hpp"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Type.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Linkage.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Lex/Lexer.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "dprint.hpp"
#include "selectfile.hpp"
#include "sema_fabricator.hpp"

using namespace clang;

#include "clang_header_includes.h"

extern const char* loc_typ_strs[NUM_LOC_TYPES];

const std::string file_get_contents(FILE *fp)
{
    char *buffer = 0;
    long length;
    fseek (fp, 0, SEEK_END);
    length = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    buffer = (char *) malloc(length);
    if (buffer)
    {
        fread (buffer, 1, length, fp);
    }
    return std::string(buffer);
}

const char *loc_typ_strs[NUM_LOC_TYPES] = {"begin func", "return", "multiline return", "exit"};

static std::vector<inst_loc *> inst_locs;
std::vector<std::string> files_to_go;
std::vector<std::string> files_skipped;

void makeFuncAndTimerNames(FunctionDecl *func, ASTContext *context, SourceManager &src_mgr, std::string &func_name,
                         std::string &timer_name);

// Exit with an error naming `file` and the reason when `failed`.
static void exit_if_io_failed(bool failed, const char *what, const std::string &file)
{
    if (failed)
    {
        const int err = errno;
        llvm::errs() << "ERROR: Could not " << what << " " << file << ": " << std::strerror(err) << "\n";
        std::exit(EXIT_FAILURE);
    }
}

// only works for inst_locs that are in the same file
static bool comp_inst_loc(inst_loc *first, inst_loc *second)
{
    if (first->line != second->line)
    {
        return first->line < second->line;
    }
    else if (first->col != second->col)
    {
        return first->col < second->col;
    }
    else
    { // SOME PEOPLE have functions that are just {} so we need to make sure begin comes before return
        return first->kind < second->kind;
    }
}

static bool eq_inst_loc(inst_loc *first, inst_loc *second)
{
    if (first->line == second->line && first->col == second->col && first->kind == second->kind &&
        strcmp(first->func_name, second->func_name) == 0)
    {
        return true;
    }
    return false;
}

void dump_inst_loc(inst_loc *loc)
{
    DPRINT("\tLine:                 %d\n", loc->line);
    DPRINT("\tCol:                    %d\n", loc->col);
    DPRINT("\tKind:                 %s\n", loc_typ_strs[loc->kind]);
    DPRINT("\tRet type:         %s\n", loc->return_type);
    DPRINT("\tName:                 \"%s\"\n", loc->func_name);
    DPRINT("\tTimer:                    %s\n", loc->full_timer_name);
    DPRINT("\tHas args:         %s\n", loc->has_args ? "Yes" : "No");
    DPRINT("\tIs ret ptr:     %s\n", loc->is_return_ptr ? "Yes" : "No");
    DPRINT("\tNeeds move:     %s\n", loc->needs_move ? "Yes" : "No");
    DPRINT("\tSkip:                 %s\n", loc->skip ? "Yes" : "No");
}

void dump_inst_loc(inst_loc *loc, int n)
{
    DPRINT("location %d\n", n);
    dump_inst_loc(loc);
}

void dump_inst_loc(inst_loc *loc, int n, bool (*filter)(inst_loc *))
{
    if (filter(loc))
    {
        DPRINT("location %d\n", n);
        dump_inst_loc(loc);
    }
}

void dump_all_locs()
{
    for (int i = 0; i < inst_locs.size(); i++)
    {
        dump_inst_loc(inst_locs[i], i);
    }
}

void dump_all_locs(std::vector<inst_loc *> locs)
{
    for (int i = 0; i < locs.size(); i++)
    {
        dump_inst_loc(locs[i], i);
    }
}

void dump_all_locs(bool (*filter)(inst_loc *))
{
    for (int i = 0; i < inst_locs.size(); i++)
    {
        dump_inst_loc(inst_locs[i], i, filter);
    }
}

std::string ReplacePhrase(std::string str, std::string phrase, std::string to_replace)
{
    while (str.find(phrase) != std::string::npos)
    {
        str.replace(str.find(phrase), phrase.length(), to_replace);
    }
    return str;
}

// Appends the main code lines `lines` to `code`
static void append_main_code(ryml::ConstNodeRef lines, inst_loc *loc, std::string &code)
{
    for (ryml::ConstNodeRef const& child : lines.children())
    {
        std::stringstream ss;
        ss << child.val();
        code += ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name) + "\n";
    }
}

void make_begin_func_code(inst_loc *loc, std::string &code, ryml::Tree yaml_tree, const bool use_cxx_api=false)
{
    /* dump the location */
    /* dump_inst_loc(loc); */
    if (strcmp(loc->func_name, "main") == 0)
    {
        // main gets its setup code even when the SIF removes its timer. A
        // main without parameters gets main_setup_insert_noargs if given.
        ryml::ConstNodeRef root = yaml_tree.crootref();
        ryml::ConstNodeRef setup;
        if (!loc->has_args)
        {
            setup = root.find_child("main_setup_insert_noargs");
        }
        if (setup.invalid())
        {
            setup = root.find_child("main_setup_insert");
        }
        if (!setup.invalid())
        {
            append_main_code(setup, loc, code);
        }
    }
    if (!loc->skip)
    {
        if (strcmp(loc->func_name, "main") == 0 )
        {
            // Insert main's timer
            append_main_code(yaml_tree[use_cxx_api ? "main_insert_scope" : "main_insert"], loc, code);
        }
        else
        {
            // Insert on function begin insert
            for (ryml::NodeRef const& child : yaml_tree[use_cxx_api ? "function_begin_insert_scope" : "function_begin_insert"].children())
            {
                std::stringstream ss;
                ss << child.val();
                std::string updated_str;
                updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                code += updated_str + "\n";
            }
        }
    }
}

// returns true if we can/should skip putting the line after this into the inst file
void make_end_func_code(inst_loc *loc, std::string &code, std::string &line, ryml::Tree yaml_tree)
{
    if (line.find("return", loc->col) == std::string::npos)
    {
        // don't put anything in for non-void functions missing explicit return
        // if we have int main() with no explicit return, use -optDisable hack
        // -glares at cmake-
        if (std::string(loc->return_type).find("void") == std::string::npos)
        {
            return;
        }
        else
        {
            // if it is void, put in stop just in case
            // Insert on function begin insert
            for (ryml::NodeRef const& child : yaml_tree["function_end_insert"].children())
            {
                std::stringstream ss;
                ss << child.val();
                std::string updated_str;
                updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                code += "\t" + updated_str + "\n";
            }
            return;
        }
    }
    // types are harder, need to pull the arg to return before the stop in case it does things
    else
    {
        std::string temp = line;
        temp.erase(std::remove_if(temp.begin(), temp.end(), isspace), temp.end());
        if (temp.find("return;") != std::string::npos)
        {
            // also throw in brackets in case SOMEONE didn't put brackets around their if
            // Insert on function begin insert
            for (ryml::NodeRef const& child : yaml_tree["function_end_insert"].children())
            {
                std::stringstream ss;
                ss << child.val();
                std::string updated_str;
                updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                code += "\t{" + updated_str + " ";
            }
            code += "return;}\n";
            return;
        }
        else
        {
            // special case void*
            if (std::string(loc->return_type).find("void") != std::string::npos && !loc->is_return_ptr)
            {
                code += "\t{ ";
                int first_pos = line.find("return") + 6;
                int last_pos = line.find(";", first_pos);
                code += line.substr(first_pos, last_pos - first_pos + 1);

                // Insert on function begin insert
                for (ryml::NodeRef const& child : yaml_tree["function_end_insert"].children())
                {
                    std::stringstream ss;
                    ss << child.val();
                    std::string updated_str;
                    updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                    code += " " + updated_str + " ";
                }
                code += "return; }\n";
            }
            // special case if we need to throw in a std::move because of copy assign shenanigans
            else if (loc->needs_move)
            {
                code += "\t{ ";
                code += loc->return_type;
                code += " inst_ret_val = std::move(";
                int first_pos = line.find("return") + 6;
                int last_pos = line.find(";", first_pos);
                // no +1, don't want semicolon
                code += line.substr(first_pos, last_pos - first_pos);
                code += "); ";

                 // Insert on function begin insert
                for (ryml::NodeRef const& child : yaml_tree["function_end_insert"].children())
                {
                    std::stringstream ss;
                    ss << child.val();
                    std::string updated_str;
                    updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                    code += " " + updated_str + " ";
                }
                code += "return inst_ret_val; }\n";
            }
            // general case for typed returns
            else
            {
                code += "\t{ ";
                code += loc->return_type;
                code += " inst_ret_val = ";
                int first_pos = line.find("return") + 6;
                int last_pos = line.find(";", first_pos);
                // +1 to catch the semicolon too
                code += line.substr(first_pos, last_pos - first_pos + 1);

                 // Insert on function begin insert
                for (ryml::NodeRef const& child : yaml_tree["function_end_insert"].children())
                {
                    std::stringstream ss;
                    ss << child.val();
                    std::string updated_str;
                    updated_str  = ReplacePhrase(ss.str(), "${full_timer_name}", loc->full_timer_name);
                    code += " " + updated_str + " ";
                }
                code += "return inst_ret_val; }\n";
            }
            return;
        }
    }
}

// The name that SIF routine entries match: the timer name before its
// source location, e.g. "void f(int)" for "void f(int)  [{f.c} ...]".
static std::string selectName(const std::string &timer_name)
{
    std::string name = timer_name.substr(0, timer_name.find(" [{"));
    name.erase(name.find_last_not_of(' ') + 1);
    return name;
}

// returns true if function matches something in list
bool check_func_against_list(std::list<std::string> list, FunctionDecl *func, ASTContext *context,
                             SourceManager &src_mgr)
{
    std::string func_name;
    std::string timer_name;
    makeFuncAndTimerNames(func, context, src_mgr, func_name, timer_name);
    return routineInList(list, selectName(timer_name));
}

// Strip the elaborations the semantic type printer introduces
static std::string cleanSemanticTypeName(std::string name)
{
    while (name.find("class ") != std::string::npos)
    {
        name.erase(name.find("class "), 6);
    }
    while (name.find("enum ") != std::string::npos)
    {
        name.erase(name.find("enum "), 5);
    }
    while (name.find("_Bool") != std::string::npos)
    {
        name.replace(name.find("_Bool"), 5, "bool");
    }
    return name;
}

// Extract the raw source text of a declarator with its name token spliced out
static std::optional<std::string> extractDeclaratorText(SourceRange range,
                                                        SourceLocation name_loc,
                                                        SourceLocation cut_loc,
                                                        const SourceManager &src_mgr,
                                                        const LangOptions &lang_opts)
{
    if (range.isInvalid())
    {
        return std::nullopt;
    }
    bool failed = false;
    auto charText = [&](SourceLocation begin, SourceLocation end) -> std::string
    {
        bool invalid = false;
        llvm::StringRef text = Lexer::getSourceText(
            CharSourceRange::getCharRange(begin, end), src_mgr, lang_opts, &invalid);
        failed = failed || invalid;
        return text.str();
    };
    SourceLocation end_loc = cut_loc.isValid()
                                 ? cut_loc
                                 : Lexer::getLocForEndOfToken(range.getEnd(), 0, src_mgr, lang_opts);
    if (end_loc.isInvalid())
    {
        return std::nullopt;
    }
    std::string raw;
    if (name_loc.isInvalid())
    {
        raw = charText(range.getBegin(), end_loc);
    }
    else
    {
        raw = charText(range.getBegin(), name_loc);
        SourceLocation after_name = Lexer::getLocForEndOfToken(name_loc, 0, src_mgr, lang_opts);
        if (after_name.isValid() && src_mgr.isBeforeInTranslationUnit(after_name, end_loc))
        {
            raw += charText(after_name, end_loc);
        }
    }
    if (failed)
    {
        return std::nullopt;
    }
    return raw;
}

// Strip comments to remove comments inline in function declaration
static std::string stripComments(const std::string &text)
{
    std::string result;
    for (size_t i = 0; i < text.size();)
    {
        if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
        {
            size_t end = text.find("*/", i + 2);
            i = (end == std::string::npos) ? text.size() : end + 2;
            result += ' ';
            continue;
        }
        if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
        {
            size_t end = text.find('\n', i + 2);
            i = (end == std::string::npos) ? text.size() : end;
            result += ' ';
            continue;
        }
        result += text[i++];
    }
    return result;
}

// Collapse whitespace runs to single spaces
static std::string collapseWhitespace(const std::string &text)
{
    std::string result;
    bool in_whitespace = false;
    for (char c : text)
    {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            in_whitespace = true;
            continue;
        }
        if (in_whitespace && !result.empty())
        {
            result += ' ';
        }
        in_whitespace = false;
        result += c;
    }
    return result;
}

// Escape '\' and '"' so the text can be embedded in generated TAU_PROFILE_TIMER("...") string literal.
static std::string escapeForStringLiteral(const std::string &text)
{
    std::string result;
    for (char c : text)
    {
        if (c == '\\' || c == '"')
        {
            result += '\\';
        }
        result += c;
    }
    return result;
}

// Reconstruct the source-spelled text of a declarator's type
static std::optional<std::string> spelledTypeText(SourceRange range,
                                                  SourceLocation name_loc,
                                                  SourceLocation cut_loc,
                                                  const SourceManager &src_mgr,
                                                  const LangOptions &lang_opts)
{
    std::optional<std::string> raw =
        extractDeclaratorText(range, name_loc, cut_loc, src_mgr, lang_opts);
    if (!raw)
    {
        return std::nullopt;
    }
    std::string clean = collapseWhitespace(stripComments(*raw));
    // Drop a trailing '=' left over from a removed default argument
    while (!clean.empty() && (clean.back() == '=' || clean.back() == ' '))
    {
        clean.pop_back();
    }
    if (clean.empty())
    {
        return std::nullopt;
    }
    return escapeForStringLiteral(clean);
}

static std::optional<std::string> spelledParamType(const ParmVarDecl *param,
                                                   const SourceManager &src_mgr,
                                                   const LangOptions &lang_opts)
{
    SourceLocation name_loc;
    if (!param->getDeclName().isEmpty())
    {
        name_loc = param->getLocation();
    }
    SourceLocation cut_loc;
    if (param->hasDefaultArg() || param->hasUnparsedDefaultArg() ||
        param->hasUninstantiatedDefaultArg())
    {
        SourceRange default_range = param->getDefaultArgRange();
        if (default_range.isValid())
        {
            cut_loc = default_range.getBegin();
        }
    }
    return spelledTypeText(param->getSourceRange(), name_loc, cut_loc, src_mgr, lang_opts);
}

static std::string spelledOrSemantic(std::optional<std::string> spelled, QualType semantic)
{
    if (spelled && !spelled->empty())
    {
        return *spelled;
    }
    return cleanSemanticTypeName(semantic.getAsString());
}

// Assemble a signature using spelled text for the return type
// when the function is invalid and for each
// parameter that is invalid or belongs to an invalid function.
static std::string spelledSignature(FunctionDecl *func, SourceManager &src_mgr,
                                    const LangOptions &lang_opts)
{
    const bool func_invalid = func->isInvalidDecl();
    std::string ret_str = spelledOrSemantic(
        func_invalid ? spelledTypeText(func->getReturnTypeSourceRange(), SourceLocation(),
                                       SourceLocation(), src_mgr, lang_opts)
                     : std::nullopt,
        func->getReturnType());
    std::string params;
    for (const ParmVarDecl *param : func->parameters())
    {
        if (!params.empty())
        {
            params += ", ";
        }
        params += spelledOrSemantic((func_invalid || param->isInvalidDecl())
                                        ? spelledParamType(param, src_mgr, lang_opts)
                                        : std::nullopt,
                                    param->getType());
    }
    if (params.empty())
    {
        params = func->isVariadic() ? "..." : "void";
    }
    else if (func->isVariadic())
    {
        params += ", ...";
    }
    const char *sep = (!ret_str.empty() && (ret_str.back() == '*' || ret_str.back() == '&'))
                          ? ""
                          : " ";
    return ret_str + sep + "(" + params + ")";
}

static bool useSpelledSignature(const FunctionDecl *func)
{
    if (!fabricate_unknown_types)
    {
        return false;
    }
    if (func->isInvalidDecl())
    {
        return true;
    }
    for (const ParmVarDecl *param : func->parameters())
    {
        if (param->isInvalidDecl())
        {
            return true;
        }
    }
    return false;
}

void makeFuncAndTimerNames(FunctionDecl *func, ASTContext *context, SourceManager &src_mgr, std::string &func_name,
                         std::string &timer_name)
{
    // PDT TAU instrumentor convention: timer source-range spans from the
    // function declaration head (return type / signature start) to the
    // closing brace of the body (utils/tau_instrumentor.cpp:200-209 in TAU
    // uses fatItem->headBegin() -> bodyEnd()).  Using getBody() for the
    // start would put start_col on the opening `{` and miss the signature.
    Stmt *func_body = func->getBody();
    SourceRange body_range = func_body->getSourceRange();

    FullSourceLoc start_loc = context->getFullLoc(func->getSourceRange().getBegin());
    FullSourceLoc end_loc = context->getFullLoc(body_range.getEnd());

    unsigned int start_line = start_loc.getSpellingLineNumber();
    unsigned int start_col = start_loc.getSpellingColumnNumber();
    unsigned int end_line = end_loc.getSpellingLineNumber();
    unsigned int end_col = end_loc.getSpellingColumnNumber();

    func_name = func->getQualifiedNameAsString();
    QualType type = func->getType();
    std::string sig;
    if (useSpelledSignature(func))
    {
        sig = spelledSignature(func, src_mgr, context->getLangOpts());
    }
    else
    {
        sig = type.getAsString();
        while (sig.find("class ") != std::string::npos)
        {
            sig.erase(sig.find("class "), 6);
        }
        while (sig.find("enum ") != std::string::npos)
        {
            sig.erase(sig.find("enum "), 5);
        }
        // not sure we need this yet
        // while (sig.find("struct ") != std::string::npos) {
        //     sig.erase(sig.find("struct "), 7);
        // }
        while (sig.find("_Bool") != std::string::npos)
        {
            // printf("%s\n", sig.c_str());
            sig.replace(sig.find("_Bool"), 5, "bool");
        }
    }
    sig.insert(sig.find_first_of("("), func_name);

    // unused variable
    // LanguageLinkage lang = func->getLanguageLinkage();
    std::string lang_string = "";
    // Kevin said this is unnecessary -- but selectfiles have it at the end so ???
    // if (lang == CLanguageLinkage) {
    //     lang_string = "C";
    // }
    // else if (lang == CXXLanguageLinkage) {
    //     lang_string = "C++";
    // }
    // else {
    //     lang_string = "unknown";
    // }

    std::string current_file = src_mgr.getFilename(start_loc).str();

    timer_name = sig + " " + lang_string + " [{" + current_file + "} {" + std::to_string(start_line) + "," +
               std::to_string(start_col) + "}-{" + std::to_string(end_line) + "," + std::to_string(end_col) + "}]";
}

class FindReturnVisitor : public RecursiveASTVisitor<FindReturnVisitor>
{
    ASTContext *context;
    SourceManager &src_mgr;
    FunctionDecl *encl_function;
    std::vector<SourceRange> lambda_locs;

  public:
    explicit FindReturnVisitor(ASTContext *context, SourceManager &SM) : context(context), src_mgr(SM)
    {
    }

    bool VisitReturnStmt(ReturnStmt *ret)
    {
        for (SourceRange lambda : lambda_locs)
        {
            if (lambda.fullyContains(ret->getSourceRange()))
            {
                // ignore lambdas
                return true;
            }
        }
        makeRetInstLoc(ret);
        return true;
    }

    bool VisitLambdaExpr(LambdaExpr *lambda)
    {
        lambda_locs.push_back(lambda->getSourceRange());
        return true;
    }

  private:
    void makeRetInstLoc(ReturnStmt *retstmt)
    {
        SourceRange range = retstmt->getSourceRange();

        FullSourceLoc start_loc = context->getFullLoc(range.getBegin());
        FullSourceLoc end_loc = context->getFullLoc(range.getEnd());

        unsigned int start_line = start_loc.getSpellingLineNumber();
        unsigned int start_col = start_loc.getSpellingColumnNumber();
        unsigned int end_line = end_loc.getSpellingLineNumber();
        // unused
        // unsigned int end_col = end_loc.getSpellingColumnNumber();

        std::string func_name;
        std::string timer_name;
        makeFuncAndTimerNames(encl_function, context, src_mgr, func_name, timer_name);

        char *func_name_c = new char[func_name.length() + 1];
        std::strcpy(func_name_c, func_name.c_str());

        char *timer_name_c = new char[timer_name.length() + 1];
        std::strcpy(timer_name_c, timer_name.c_str());

        std::string ret_name = encl_function->getReturnType().getAsString();
        if (encl_function->getReturnType().getTypePtr()->isBooleanType() && ret_name.find("_Bool") != std::string::npos)
        {
            ret_name.replace(ret_name.find("_Bool"), 5, "bool");
        }
        if (ret_name.substr(0, 5).find("class") != std::string::npos)
        {
            ret_name = ret_name.substr(6); // if it starts with "class", chop that off
        }
        if (ret_name.substr(0, 11).find("const class") != std::string::npos)
        {
            ret_name = ret_name.erase(6, 6); // if it starts with "class", chop that off
        }

        bool needs_move = false;
        if (encl_function->getReturnType()->isClassType())
        {
            CXXRecordDecl *decl = encl_function->getReturnType()->getAsCXXRecordDecl();
            // Check whether there's actually a definition.
            // A fabricated type won't have one.
            if (decl != nullptr && decl->hasDefinition() &&
                !(decl->hasSimpleCopyAssignment() || decl->hasTrivialCopyAssignment()))
            {
                needs_move = true;
            }
        }

        char *ret_name_c = new char[ret_name.length() + 1];
        std::strcpy(ret_name_c, ret_name.c_str());

        inst_loc *ret = new inst_loc;
        ret->line = start_line;
        ret->col = start_col - 1;
        ret->kind = start_line == end_line ? RETURN_FUNC : MULTILINE_RETURN_FUNC;
        ret->func_name = func_name_c;
        ret->return_type = ret_name_c;
        ret->full_timer_name = timer_name_c;
        ret->has_args = encl_function->getNumParams() > 0;
        ret->is_return_ptr = encl_function->getReturnType()->isPointerType();
        ret->needs_move = needs_move;

        inst_locs.push_back(ret);

        // llvm::outs() << "\tFound return at " << start_line << ":" << start_col << "\n";
    }

    friend class FindFunctionVisitor;
};

class FindFunctionVisitor : public RecursiveASTVisitor<FindFunctionVisitor>
{
    ASTContext *context;
    SourceManager &src_mgr;
    FindReturnVisitor return_visitor;

  public:
    explicit FindFunctionVisitor(ASTContext *context, SourceManager &SM)
        : context(context), src_mgr(SM), return_visitor(context, SM)
    {
    }

    bool VisitFunctionDecl(FunctionDecl *func)
    {
        // if (func->isInlined()) {
        //     printf("Function %s is inline\n", func->getQualifiedNameAsString().c_str());
        // }
        // short circuit on hasBody() first to protect check_func_against_list (and makeFuncInstLoc) from segfaults
        if (func->hasBody() &&
            (!func->isInlined() || do_inline || check_func_against_list(includelist, func, context, src_mgr)))
        { //
            makeFuncInstLoc(func);
            return_visitor.encl_function = func;
            return_visitor.TraverseDecl(func);
        }
        return true;
    }

  private:
    void makeFuncInstLoc(FunctionDecl *func)
    {
        Stmt *func_body = func->getBody();
        SourceRange range = func_body->getSourceRange();

        FullSourceLoc start_loc = context->getFullLoc(range.getBegin());
        FullSourceLoc end_loc = context->getFullLoc(range.getEnd());

        unsigned int start_line = start_loc.getSpellingLineNumber();
        unsigned int start_col = start_loc.getSpellingColumnNumber();
        unsigned int end_line = end_loc.getSpellingLineNumber();
        unsigned int end_col = end_loc.getSpellingColumnNumber();

        std::string func_name;
        std::string timer_name;

        makeFuncAndTimerNames(func, context, src_mgr, func_name, timer_name);

        char *func_name_c = new char[func_name.length() + 1];
        std::strcpy(func_name_c, func_name.c_str());

        char *timer_name_c = new char[timer_name.length() + 1];
        std::strcpy(timer_name_c, timer_name.c_str());

        std::string ret_name = func->getReturnType().getAsString();
        // why
        if (func->getReturnType().getTypePtr()->isBooleanType() && ret_name.find("_Bool") != std::string::npos)
        {
            ret_name.replace(ret_name.find("_Bool"), 5, "bool ");
        }
        if (ret_name.substr(0, 5).find("class") != std::string::npos)
        {
            ret_name = ret_name.substr(6); // if it starts with "class", chop that off
        }
        if (ret_name.substr(0, 11).find("const class") != std::string::npos)
        {
            ret_name = ret_name.erase(6, 6); // if it starts with "class", chop that off
        }
        char *ret_name_c = new char[ret_name.length() + 1];
        std::strcpy(ret_name_c, ret_name.c_str());

        bool needs_move = false;
        if (func->getReturnType()->isClassType())
        {
            CXXRecordDecl *decl = func->getReturnType()->getAsCXXRecordDecl();
            // Check whether there's actually a definition.
            // A fabricated type won't have one.
            if (decl != nullptr && decl->hasDefinition() &&
                !(decl->hasSimpleCopyAssignment() || decl->hasTrivialCopyAssignment()))
            {
                needs_move = true;
            }
        }

        inst_loc *start = new inst_loc;
        start->line = start_line;
        start->col = start_col;
        start->kind = BEGIN_FUNC;
        start->return_type = ret_name_c;
        start->func_name = func_name_c;
        start->full_timer_name = timer_name_c;
        start->has_args = func->getNumParams() > 0;
        start->is_return_ptr = func->getReturnType()->isPointerType();
        start->needs_move = needs_move;

        inst_locs.push_back(start);

        inst_loc *end = new inst_loc;
        end->line = end_line;
        end->col = end_col - 1;
        end->kind = RETURN_FUNC;
        end->return_type = ret_name_c;
        end->func_name = func_name_c;
        end->full_timer_name = timer_name_c;
        end->has_args = func->getNumParams() > 0;
        end->is_return_ptr = func->getReturnType()->isPointerType();
        end->needs_move = needs_move;

        inst_locs.push_back(end);

        // llvm::outs() << "Found function " << timer_name << "\n";
    }
};

class FindFunctionConsumer : public clang::ASTConsumer
{
    FindFunctionVisitor func_visitor;
    SourceManager &src_mgr;

  public:
    FindFunctionConsumer(ASTContext *context, SourceManager &SM) : func_visitor(context, SM), src_mgr(SM)
    {
    }

    virtual void HandleTranslationUnit(ASTContext &context)
    {
        auto decls = context.getTranslationUnitDecl()->decls();
        for (auto &decl : decls)
        {
            // Implicit decls don't have source locations
            if (decl->isImplicit())
            {
                continue;
            }
            SourceLocation srcloc = decl->getLocation();
            // always exclude system headers
            if (src_mgr.isInSystemHeader(srcloc) ||
                src_mgr.isInExternCSystemHeader(srcloc) ||
                src_mgr.isInSystemMacro(srcloc))
            {
                continue;
            }
            // findFiles() applies the file lists to the source files
            func_visitor.TraverseDecl(decl);
        }
    }
};

class FindFunctionAction : public ASTFrontendAction
{
  public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, llvm::StringRef InFile) override
    {
        return std::make_unique<FindFunctionConsumer>(&Compiler.getASTContext(), Compiler.getSourceManager());
    }

    void ExecuteAction() override
    {
        if (!fabricate_unknown_types)
        {
            ASTFrontendAction::ExecuteAction();
            return;
        }
        CompilerInstance &CI = getCompilerInstance();
        if (!CI.hasPreprocessor())
        {
            return;
        }
        if (!CI.hasSema())
        {
            CI.createSema(getTranslationUnitKind(), nullptr);
        }
        fabricator = llvm::makeIntrusiveRefCnt<salt::SaltSemaFabricator>();
        fabricator->InitializeSema(CI.getSema());
#if LLVM_VERSION_MAJOR >= 22
        CI.getSema().addExternalSource(fabricator);
#else
        CI.getSema().addExternalSource(fabricator.get());
#endif
        ParseAST(CI.getSema(), CI.getFrontendOpts().ShowStats,
                 CI.getFrontendOpts().SkipFunctionBodies);
        llvm::outs() << "Fabricated " << fabricator->numNamespaces()
                     << " namespaces, " << fabricator->numTemplates()
                     << " class templates, " << fabricator->numRecords()
                     << " records\n";
    }

  private:
    llvm::IntrusiveRefCntPtr<salt::SaltSemaFabricator> fabricator;
};

instrumentor::instrumentor()
{
    // Constructor currently does not need to do anything
}

void instrumentor::set_exec_name(const char* name)
{
    exec_name = strdup(name);
}

int instrumentor::run_tool()
{
    return Tool->run(tooling::newFrontendActionFactory<FindFunctionAction>().get());
}

void instrumentor::select_routines()
{
    for (inst_loc *loc : inst_locs)
    {
        const std::string name = selectName(loc->full_timer_name);
        loc->skip = !instrumentRoutine(name);
        if (main_setup_level != MainSetupLevel::never && loc->kind == BEGIN_FUNC &&
            strcmp(loc->func_name, "main") == 0)
        {
            warnIfMainNotIncluded(name);
        }
    }
}

void instrumentor::instrument_file(std::ifstream &og_file, std::ofstream &inst_file, std::string filename,
                     std::vector<inst_loc *> inst_locations, bool use_cxx_api, ryml::Tree yaml_tree)
{
    std::string line;
    int lineno = 0;

    auto inst_loc_iter = inst_locations.begin();

    for (ryml::NodeRef const& child : yaml_tree["include"].children()) {
        inst_file << "#include " << child.val() << "\n";
    }

    inst_file << "#line 1 \"" << filename << "\"\n";

    while (getline(og_file, line))
    {
        lineno++;
        // short circuit if we run out of inst locations to avoid segfaults :)
        // need the if and the while because sometimes 2+ inst locations are on the same line
        if (inst_loc_iter != inst_locations.end())
        {
            inst_loc *curr_inst_loc = *inst_loc_iter;
            if (lineno == curr_inst_loc->line)
            {
                curr_inst_loc = *inst_loc_iter;
                int num_inst_locs_this_line = 0;
                std::string start = line.substr(0, curr_inst_loc->col);
                std::string end;
                while (inst_loc_iter != inst_locations.end() && lineno == curr_inst_loc->line)
                {
                    if (curr_inst_loc->col < line.size())
                    {
                        end = line.substr(curr_inst_loc->col);
                    }
                    else
                    {
                        end = "";
                    }
                    if (num_inst_locs_this_line == 0)
                    {
                        inst_file << start;
                    }
                    std::string inst_code = "";
                    switch (curr_inst_loc->kind)
                    {
                    case BEGIN_FUNC:
                        inst_file << "\n#line " << lineno << "\n";
                        make_begin_func_code(curr_inst_loc, inst_code, yaml_tree, use_cxx_api);
                        inst_file << inst_code;
                        inst_file << "#line " << lineno << "\n";
                        break;
                    case RETURN_FUNC:
                        if (!use_cxx_api)
                        {
                            inst_file << "\n#line " << lineno << "\n";
                            make_end_func_code(curr_inst_loc, inst_code, line, yaml_tree);
                            inst_file << inst_code;
                            inst_file << "#line " << lineno << "\n";
                            if (line.find("return", curr_inst_loc->col) != std::string::npos &&
                                line.find(";", line.find("return", curr_inst_loc->col)) != std::string::npos)
                            {
                                end = line.substr(line.find(";", line.find("return", curr_inst_loc->col)) + 1);
                            }
                        }
                        break;
                    case MULTILINE_RETURN_FUNC:
                        if (!use_cxx_api)
                        {
                            inst_file << "\n#line " << lineno << "\n";
                            // join all the lines together into one so make_end_func_code can actually know what's
                            // happening
                            while (line.find(";") == std::string::npos)
                            {
                                std::string templine;
                                getline(og_file, templine);
                                line += templine;
                                lineno++;
                            }
                            make_end_func_code(curr_inst_loc, inst_code, line, yaml_tree);
                            inst_file << inst_code;
                            inst_file << "#line " << lineno << "\n";
                            if (line.find("return", curr_inst_loc->col) != std::string::npos &&
                                line.find(";", line.find("return", curr_inst_loc->col)) != std::string::npos)
                            {
                                end = line.substr(line.find(";", line.find("return", curr_inst_loc->col)) + 1);
                            }
                        }
                    default:
                        break;
                    }

                    inst_loc_iter++;
                    curr_inst_loc = *inst_loc_iter;
                    num_inst_locs_this_line++;
                }

                inst_file << end << "\n";
            }
            else
            {
                inst_file << line << "\n";
            }
        }
        else
        {
            inst_file << line << "\n";
        }
    }
}

// The output file for source `fname`: --tau_output if given, else the
// basename of `fname` with ".inst" before its extension, in the working
// directory.
static std::string inst_file_name(const std::string &fname)
{
    if (!outputfile.empty())
    {
        return outputfile;
    }
    std::string newname = fname.substr(fname.find_last_of("/\\") + 1);
    const auto location = newname.find_last_of(".");
    newname.insert(location == std::string::npos ? newname.size() : location, ".inst");
    return newname;
}

// `path` made absolute and without `.` or `..` components, so that a
// source named on the command line compares equal to Clang's name for it
static std::string normal_path(const std::string &path)
{
    llvm::SmallString<256> result(path);
    llvm::sys::fs::make_absolute(result);
    llvm::sys::path::remove_dots(result, /*remove_dot_dot=*/true);
    return std::string(result);
}

void instrumentor::instrument()
{
    // printf("size %zu\n", files_to_go.size());
    // for (std::string fname : files_to_go) {
    //     printf("Instrumenting %s\n", fname.c_str());
    // }

    // The file lists exclude the files after files_to_go, but main keeps
    // its setup code there
    std::vector<std::string> files = files_to_go;
    files.insert(files.end(), files_skipped.begin(), files_skipped.end());
    for (size_t i = 0; i < files.size(); i++)
    {
        const std::string &fname = files[i];
        const bool skipped = i >= files_to_go.size();
        DPRINT("Instrumenting %s\n", fname.c_str());
        std::vector<inst_loc *> inst_locations;
        const std::string abs_fname = normal_path(fname);

        DPRINT0("filtering locations for file\n");
        // filter on fname
        for (inst_loc *loc : inst_locs)
        {
            std::string timer_name = std::string(loc->full_timer_name);
            // The file name, between " [{" and the "} {" before the line
            // numbers, which may itself hold braces
            const size_t fname_start = timer_name.find(" [{") + 3;
            std::string loc_fname = timer_name.substr(fname_start, timer_name.rfind("} {") - fname_start);
            // printf("loc file %s\n", loc_fname.c_str());
            // printf("cur file %s\n", fname.c_str());
            if (normal_path(loc_fname) == abs_fname)
            {
                loc->skip = loc->skip || skipped;
                // A routine the lists leave out keeps its lines as written,
                // except that main gets its setup code above the `never` level
                if (loc->skip && (main_setup_level == MainSetupLevel::never || loc->kind != BEGIN_FUNC ||
                                  strcmp(loc->func_name, "main") != 0))
                {
                    continue;
                }
                inst_locations.push_back(loc);
            }
        }

        // inst_locs.begin(), inst_locs.end(), inst_locations.begin(),
        // [&](inst_loc* loc) {
        //     std::string timer_name = std::string(loc->full_timer_name);
        //     // grab contents of first set of curly braces, which is filename
        //     std::string loc_fname = timer_name.substr(timer_name.find_first_of("{")+1, timer_name.find_first_of("}") -
        //     timer_name.find_first_of("{")-1); printf("loc file %s\n", loc_fname.c_str()); printf("cur file %s\n",
        //     fname.c_str()); return trim(loc_fname) == trim(fname);
        // });
        // sort by line numbers then cols (then loc type) so looping goes well
        std::sort(inst_locations.begin(), inst_locations.end(), comp_inst_loc);
        // unique again just in case
        auto new_end3 = std::unique(inst_locations.begin(), inst_locations.end(), eq_inst_loc);
        inst_locations.erase(new_end3, inst_locations.end());

        // if (fname.find("WaveFunction.cpp") != std::string::npos) {
        // dump_all_locs(inst_locations);
        // }

        std::ifstream og_file;
        std::ofstream inst_file;
        const std::string newname = inst_file_name(fname);
        DPRINT("new filename (inst): %s\n", newname.c_str());

        // Open the input first, so a missing one leaves no output file
        og_file.open(fname);
        exit_if_io_failed(!og_file.is_open(), "open input file", fname);
        inst_file.open(newname);
        exit_if_io_failed(!inst_file.is_open(), "open output file", newname);

        if (skipped && inst_locations.empty())
        {
            // __FILE__ and debug information name the source, not the copy
            inst_file << "#line 1 \"" << fname << "\"\n";
            // Copying an empty rdbuf() sets failbit, so copy only a non-empty file
            if (og_file.peek() != std::ifstream::traits_type::eof())
            {
                inst_file << og_file.rdbuf();
            }
            exit_if_io_failed(!inst_file, "write output file", newname);
            og_file.close();
            inst_file.close();
            exit_if_io_failed(!inst_file, "write output file", newname);
            continue;
        }

        // check for cxxparse executable name. If so, force cxx api usage
        if (strstr(exec_name, "cxxparse") != nullptr)
        {
            use_cxx_api = true;
            DPRINT("%s: Forcing TAU CXX API\n", exec_name);
            fflush(stdout);
        }

        // Read config.yaml
        ryml::Tree yaml_tree;
        if (FILE *config_file = fopen(configfile.c_str(), "r"))
        {
            std::ifstream inputStream{configfile.c_str()};
            if (!inputStream) {
                llvm::errs() << "ERROR: Could not open configuration file " << configfile.c_str() << "\n";
                std::exit(-3);
            }
            std::stringstream configStream;
            configStream << inputStream.rdbuf();
            yaml_tree = ryml::parse_in_arena(ryml::to_csubstr(configStream.str()));

            fclose(config_file);
        }
        else
        {
            llvm::outs() << "No config file found\n";
            remove(newname.c_str());
            exit(1);
        }

        // If using C++ API, check that config file contains code for scoped instrumentation
        if (use_cxx_api) {
            if (ryml::ConstNodeRef mainInsertScope = yaml_tree["main_insert_scope"]; mainInsertScope.invalid()) {
               llvm::errs() << "Using C++ Instrumentation API requires `main_insert_scope` in config file.\n";
                exit(2);
            }
            if (ryml::ConstNodeRef functionBeginScope = yaml_tree["function_begin_insert_scope"]; functionBeginScope.
                invalid()) {
                llvm::errs() << "Using C++ Instrumentation API requires `function_begin_insert_scope` in config file.\n";
                exit(2);
            }
        }

        llvm::outs() << "Instrumentation: " << yaml_tree["instrumentation"].val() << "\n";
        instrument_file(og_file, inst_file, fname, inst_locations, use_cxx_api, yaml_tree);
        exit_if_io_failed(!inst_file, "write output file", newname);
        og_file.close();
        inst_file.close();
        exit_if_io_failed(!inst_file, "write output file", newname);
    }
}
