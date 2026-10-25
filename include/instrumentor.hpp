#ifndef TOOLING_H
#define TOOLING_H

#include <ryml_all.hpp>
#include "clang/Tooling/Tooling.h"
#include <vector>

#include "selectfile.hpp"

/* defines */
#ifdef TAU_WINDOWS
#define TAU_DIR_CHARACTER '\\'
#else
#define TAU_DIR_CHARACTER '/'
#endif /* TAU_WINDOWS */

// make sure begin func comes before returns and such
#define BEGIN_FUNC 0
#define RETURN_FUNC 1
#define MULTILINE_RETURN_FUNC 2
#define EXIT_FUNC 3
#define NUM_LOC_TYPES 4


static llvm::cl::OptionCategory MyToolCategory(
    "TAU instrumentor options",
    "To pass options directly to the compiler, use\n\ttooling [options] <source> -- [compiler options]");

extern llvm::cl::opt<std::string> outputfile;

extern llvm::cl::opt<std::string> configfile;

extern llvm::cl::opt<bool> use_cxx_api;

extern llvm::cl::opt<bool> do_inline;

extern llvm::cl::opt<std::string> selectfile;

extern llvm::cl::opt<bool> fabricate_unknown_types;

extern llvm::cl::opt<ExcludedMainSetup> excluded_main_setup;

typedef struct inst_loc {
    int line = -1;
    // The line that the `#line` lines give `line`, minus `line`
    int line_shift = 0;
    int col = -1;
    int kind = -1;
    const char* return_type;
    const char* func_name;
    const char* full_timer_name;
    // The file that holds the function, not the one of `#line` lines
    const char* file;
    bool has_args = false;
    bool is_return_ptr = false;
    bool needs_move = false;
    bool skip = false;
} inst_loc;

#endif

// Source files to instrument and source files the file lists exclude,
// defined once in instrumentor.cpp and filled by frontend.cpp
extern std::vector<std::string> files_to_go;
extern std::vector<std::string> files_skipped;

class instrumentor {
public:

    clang::tooling::ClangTool* Tool = nullptr;
    char* exec_name;

    instrumentor();

    void set_exec_name(const char* name);

    // Parses the sources; returns ClangTool::run's status, non-zero on errors or skipped files.
    int run_tool();

    // Skips the timers of the routines that the routine lists do not select
    void select_routines();

    void instrument_file(std::ifstream &og_file, std::ofstream &inst_file, std::string filename,
                     std::vector<inst_loc *> inst_locations, bool use_cxx_api, ryml::Tree yaml_tree);

    void instrument();
};
