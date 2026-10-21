#ifdef __linux__
#include <unistd.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#endif
#include "instrumentor.hpp"
#include "frontend.hpp"
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
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/StringSaver.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

#include "selectfile.hpp"

using namespace clang;

llvm::cl::opt<std::string> outputfile("tau_output", llvm::cl::desc("Specify name of output instrumented file"),
                                      llvm::cl::value_desc("filename"), llvm::cl::cat(MyToolCategory));

std::string getExecutablePath() {
    char buffer[1024];
    std::string path;

    #ifdef __linux__
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        path = std::string(buffer);
    }
    #elif __APPLE__
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        path = std::string(buffer);
    } else {
        char *pathBuffer = new char[size];
        if (_NSGetExecutablePath(pathBuffer, &size) == 0) {
            path = std::string(pathBuffer);
        }
        delete[] pathBuffer;
    }
    #endif

    // Remove the executable name from the path
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
    }

    return path;
}

std::string getEnvCfgFile() {
    char *val = getenv("SALT_CONFIG_FILE");
    if (val == NULL || *val == '\0') {
        std::string execPath = getExecutablePath();
        return execPath + "/" + SALT_DEFAULT_CONFIG_FILE;
    } else {
        return std::string(val);
    }
}


llvm::cl::opt<std::string> configfile("config_file",
                                      llvm::cl::desc("Specify path to SALT configuration YAML file (default: "
                                                     "$SALT_CONFIG_FILE, else the installed config.yaml)"),
                                      llvm::cl::value_desc("filename"), llvm::cl::init(getEnvCfgFile()),
                                      llvm::cl::cat(MyToolCategory));

llvm::cl::opt<bool> use_cxx_api("tau_use_cxx_api", llvm::cl::desc("Use TAU's C++ instrumentation API"),
                                llvm::cl::init(false), llvm::cl::cat(MyToolCategory));

llvm::cl::opt<bool> do_inline("tau_instrument_inline", llvm::cl::desc("Instrument inlined functions (default: false)"),
                              llvm::cl::init(false), llvm::cl::cat(MyToolCategory));

llvm::cl::opt<std::string> selectfile("tau_select_file",
                                      llvm::cl::desc("Provide a selective instrumentation specification file"),
                                      llvm::cl::value_desc("filename"), llvm::cl::cat(MyToolCategory));

// Whether environment variable `name` is set to a true value: anything
// but empty, 0, false, no, or off, in any case.
static bool getEnvBool(const char *name) {
    const char *val = getenv(name);
    if (val == nullptr) {
        return false;
    }
    std::string s(val);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return !(s.empty() || s == "0" || s == "false" || s == "no" || s == "off");
}

llvm::cl::opt<bool> fabricate_unknown_types("tau_fabricate_unknown_types",
                                            llvm::cl::desc("Fabricate declarations for unknown qualified C++ names."),
                                            llvm::cl::init(getEnvBool("SALT_FABRICATE_UNKNOWN_TYPES")),
                                            llvm::cl::cat(MyToolCategory));

static llvm::cl::opt<bool> allow_parse_errors(
    "tau_allow_parse_errors",
    llvm::cl::desc("Instrument the partial parse tree that Clang builds after errors "
                   "(default: $SALT_ALLOW_PARSE_ERRORS, else false)"),
    llvm::cl::init(getEnvBool("SALT_ALLOW_PARSE_ERRORS")), llvm::cl::cat(MyToolCategory));

llvm::cl::opt<ExcludedMainSetup> excluded_main_setup(
    "tau_excluded_main_setup",
    llvm::cl::desc("Selects which excluded main programs get setup code"),
    llvm::cl::values(
        clEnumValN(ExcludedMainSetup::none, "none", "No excluded main program gets setup code"),
        clEnumValN(ExcludedMainSetup::routine, "routine",
                   "A main program that the routine lists exclude gets setup code (default)"),
        clEnumValN(ExcludedMainSetup::file, "file",
                   "Also a main program in a file that the file lists exclude (parses all "
                   "excluded files, adds no timers to them)")),
    llvm::cl::init(ExcludedMainSetup::routine), llvm::cl::cat(MyToolCategory));

#include "clang_header_includes.h"

char **addHeadersToCommand(int *argc, const char **argv)
{
    // first, check for no arguments.    If none, add --help
    if (*argc == 1)
    {
        *argc = 2;
        char **new_argv = (char **)(calloc(2, sizeof(char *)));
        new_argv[0] = strdup(argv[0]);
        new_argv[1] = strdup("--help");
        return new_argv;
    }
    // We need to add N include paths to the argument list, after the '--'
    int new_argc = *argc + clang_header_includes_length;
    char **new_argv = (char **)(calloc(new_argc, sizeof(char *)));
    int new_i = 0;
    bool found_dash = false; // Marker for finding "--"

    for (int i = 0; i <= *argc; i++)
    {
        if (i == *argc && found_dash) break;
        bool at_dash = false;
        if ((i == *argc) ||                             // found "--"
            (at_dash = strcmp(argv[i], "--") == 0))     // Reached end of argv without finding "--"
        {
            if (at_dash) {
                found_dash = true;
                new_argv[new_i] = strdup(argv[i]);
            }
            else
            {
                new_argv = (char **)realloc(new_argv, (++new_argc)*sizeof(char *));
                new_argv[new_i] = strdup("--");
            }
            new_i++;
            for (int j = 0; j < clang_header_includes_length; j++)
            {
                new_argv[new_i] = strdup(clang_header_includes[j]);
                new_i++;
            }
        }
        else
        {
            // ignore linker flags
            if ((strncmp(argv[i], "-L", 2) != 0) ||        // ignore library paths
                (strncmp(argv[i], "-l", 2) != 0) ||        // ignore libraries
                (strncmp(argv[i], "-D_OPENMP", 9) != 0) || // ignore expanded OpenMP macro
                (strncmp(argv[i], "-Wl", 3) != 0))
            { // ignore linker options
                new_argv[new_i] = strdup(argv[i]);
                new_i++;
            }
            else
            {
                new_argc--;
            }
        }
    }
    *argc = new_argc;
    return new_argv;
}

// Warns that `path`, which the file lists exclude and SALT-FM copies without a parse, gets no
// setup code when it seems to contain the main program: a `main` token right before a `(` token,
// from Clang's raw lexer, which skips comments, keeps each string literal one token, does
// no preprocessing, and does not fail on bad code
static void warnIfExcludedMain(const std::string &path)
{
    auto buffer = llvm::MemoryBuffer::getFile(path);
    if (!buffer)
    {
        return;
    }
    LangOptions opts;
    opts.CPlusPlus = true;
    opts.LineComment = true;
    llvm::StringRef text = (*buffer)->getBuffer();
    Lexer lexer(SourceLocation(), opts, text.begin(), text.begin(), text.end());
    Token tok;
    bool after_main = false;
    bool at_end = false;
    while (!at_end)
    {
        // The last token comes with `true` when the file ends right after it
        at_end = lexer.LexFromRawLexer(tok);
        if (after_main && tok.is(tok::l_paren))
        {
            llvm::errs() << "WARNING: " << path << " seems to contain the main program. The file lists\n"
                         << "  exclude this file, so the main program gets no setup code. To add the\n"
                         << "  setup code, use --tau_excluded_main_setup=file.\n";
            return;
        }
        after_main = tok.is(tok::raw_identifier) && tok.getRawIdentifier() == "main";
    }
}

void findFiles(const std::vector<std::string>& files)
{
    for (std::string fil : files)
    {
        if (instrumentFile(fil))
        {
            if (!fil.empty() && std::find(files_to_go.begin(), files_to_go.end(), fil) == files_to_go.end())
            {
                files_to_go.push_back(fil);
            }
        }
        else
        {
            files_skipped.push_back(fil);
        }
    }

    if (files.size() < 1)
    {
        fprintf(stderr, "ERROR: no file to instrument, exiting.");
        exit(0);
    }
}

// Refuses an output that names one of `inputs`, the configuration file, or the selective
// instrumentation file, and removes a stale output. Returns false after an error.
static bool prepareOutput(std::vector<std::string> inputs)
{
    inputs.push_back(configfile);
    inputs.push_back(selectfile);
    // Opening the output would empty an input it names, so refuse it before anything is written.
    if (std::any_of(inputs.begin(), inputs.end(),
                    [](const std::string &input) { return llvm::sys::fs::equivalent(input, outputfile); }))
    {
        llvm::errs() << "ERROR: Output file " << outputfile << " is an input file\n";
        return false;
    }
    // TAU's compiler wrapper compiles any file left at this path, even a stale one, so
    // remove it before anything can fail. Skip a read-only file; the open for writing
    // then fails and saltfm exits with an error.
    if (llvm::sys::fs::is_regular_file(outputfile) &&
        !llvm::sys::fs::access(outputfile, llvm::sys::fs::AccessMode::Write))
    {
        if (std::error_code err = llvm::sys::fs::remove(outputfile))
        {
            llvm::errs() << "ERROR: Could not remove output file " << outputfile << ": " << err.message() << "\n";
            return false;
        }
    }
    return true;
}

// The arguments that can name an input when the option parser fails and gives no source list:
// all but the value of a separate --tau_output, with response files expanded as the parser does.
// `create` has already cut `argc` at `--`, as the parser reads it.
static std::vector<std::string> argumentInputs(int argc, const char *const *argv)
{
    llvm::BumpPtrAllocator allocator;
    llvm::StringSaver saver(allocator);
    llvm::SmallVector<const char *, 20> args;
    llvm::cl::expandResponseFiles(argc, argv, nullptr, saver, args);
    std::vector<std::string> inputs;
    // shortcut: this skips the word after any --tau_output or -tau_output, even when that word is
    // the value of the option before it, as in `--tau_select_file --tau_output src.c`, so an
    // option error can remove src.c; fix it if real builds pass such arguments.
    for (size_t i = 0; i < args.size(); ++i)
    {
        llvm::StringRef previous = i > 0 ? args[i - 1] : "";
        if (previous != "--tau_output" && previous != "-tau_output")
        {
            inputs.push_back(args[i]);
        }
    }
    return inputs;
}

int main(int argc, const char **argv)
{
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &OS) {
        OS << "SALT-FM Version: " << SALT_VERSION_FULL << "\n";
        OS << "LLVM version " << LLVM_VERSION_STRING << "\n";
#ifdef NDEBUG
        OS << "  Optimized build.\n";
#else
        OS << "  Debug build.\n";
#endif
    });

    int new_argc = argc;
    char **new_argv = addHeadersToCommand(&new_argc, argv);

    //Get source paths
    auto ExpectedParser =
        tooling::CommonOptionsParser::create(new_argc, (const char **)new_argv, MyToolCategory, llvm::cl::OneOrMore,
                                             "Tool for adding TAU instrumentation to source files.\nNote that this "
                                             "will only instrument the first source file given.");

    if (!ExpectedParser)
    {
        llvm::errs() << ExpectedParser.takeError();
        // The parser sets the options that it reads before the error, so --tau_output can name a
        // stale output.
        if (outputfile.getNumOccurrences() > 0)
        {
            prepareOutput(argumentInputs(new_argc, new_argv));
        }
        return 1;
    }

    tooling::CommonOptionsParser &OptionsParser = ExpectedParser.get();
    if (!prepareOutput(OptionsParser.getSourcePathList()))
    {
        return EXIT_FAILURE;
    }
    // An empty value is not a file; SALT_CONFIG_FILE and the default never give one
    for (const llvm::cl::opt<std::string> *option : {&configfile, &outputfile, &selectfile})
    {
        if (option->getNumOccurrences() > 0 && option->empty())
        {
            llvm::errs() << "ERROR: --" << option->ArgStr << " needs a path\n";
            return EXIT_FAILURE;
        }
    }
    // processInstrumentationRequests() reports a file that it cannot read
    if (!selectfile.empty() && !processInstrumentationRequests(selectfile.c_str()))
    {
        return EXIT_FAILURE;
    }

    findFiles(OptionsParser.getSourcePathList()); //Locate source files and mark for instrumentation/skipping

    // Unless --tau_excluded_main_setup=file, parse only the sources that the file lists include
    std::vector<std::string> sources = OptionsParser.getSourcePathList();
    if (excluded_main_setup != ExcludedMainSetup::file)
    {
        sources = files_to_go;
        std::for_each(files_skipped.begin(), files_skipped.end(), warnIfExcludedMain);
    }
    instrumentor CodeInstrumentor;
    CodeInstrumentor.Tool = new tooling::ClangTool(OptionsParser.getCompilations(), sources);
    CodeInstrumentor.set_exec_name(argv[0]);

    if (CodeInstrumentor.run_tool() != 0)
    {
        // Fabrication works through Clang's error recovery, so its errors are expected.
        if (!allow_parse_errors && !fabricate_unknown_types)
        {
            return EXIT_FAILURE;
        }
        llvm::errs() << "WARNING: Clang reported errors; instrumenting the partial parse tree\n";
    }

    CodeInstrumentor.select_routines(); // Apply the routine lists

    CodeInstrumentor.instrument();

}
