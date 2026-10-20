#ifndef SELECTFILE_H
#define SELECTFILE_H

#include <list>
#include <string>

#define BEGIN_EXCLUDE_TOKEN      "BEGIN_EXCLUDE_LIST"
#define END_EXCLUDE_TOKEN        "END_EXCLUDE_LIST"
#define BEGIN_INCLUDE_TOKEN      "BEGIN_INCLUDE_LIST"
#define END_INCLUDE_TOKEN        "END_INCLUDE_LIST"
#define BEGIN_FILE_INCLUDE_TOKEN "BEGIN_FILE_INCLUDE_LIST"
#define END_FILE_INCLUDE_TOKEN   "END_FILE_INCLUDE_LIST"
#define BEGIN_FILE_EXCLUDE_TOKEN "BEGIN_FILE_EXCLUDE_LIST"
#define END_FILE_EXCLUDE_TOKEN   "END_FILE_EXCLUDE_LIST"
#define BEGIN_INSTRUMENT_SECTION "BEGIN_INSTRUMENT_SECTION"
#define END_INSTRUMENT_SECTION	 "END_INSTRUMENT_SECTION"

#define WSPACE(line) while ( line[0] == ' ' || line[0] == '\t')  \
    { \
      if (line[0] == '\0') parseError("EOL found", line, lineno, line - original);  \
      line++;  \
    }

#define TOKEN(k) if (line[0] != k || line[0] == '\0') parseError("token not found", line, lineno, (int ) (line - original)); \
		 else line++;

#define RETRIEVESTRING(pname, line) i = 0; \
       while (line[0] != '"') { \
       if (line [0] == '\0') parseError("EOL", line, lineno, line - original); \
         pname[i++] = line[0]; line++; \
       } \
       pname[i] = '\0';  \
       line++; /* found closing " */

#define RETRIEVENUMBER(pname, line) i = 0; \
  while (line[0] != ' ' && line[0] != '\t' ) { \
  if (line [0] == '\0') parseError("EOL", line, lineno, line - original); \
    pname[i++] = line[0]; line++; \
  } \
  pname[i] = '\0';  \
  line++; /* found closing " */

#define RETRIEVECODE(pname, line) i = 0; \
  while (line[0] != '"') { \
    if (line [0] == '\0') parseError("EOL", line, lineno, line - original); \
    if (line[0] == '\\') { \
      switch(line[1]) { \
        case '\\': \
        case '"': \
          break; \
        case 'n': \
          line[1] = '\n'; \
          break; \
        case 't': \
          line[1] = '\t'; \
          break; \
        default: \
          parseError("Unknown escape sequence", line, lineno, line - original); \
          break; \
      } \
      line++; \
    } \
    pname[i++] = line[0]; line++; \
  } \
  pname[i] = '\0';  \
  line++; /* found closing " */

/* When SALT-FM parses a source that the file lists exclude, and whether a
   main program that the selective instrumentation lists leave out gets
   its setup code: `never` parses only the sources that the file lists
   include and gives such a main no setup code; `function` parses the same
   sources but gives a main that the routine lists leave out its setup
   code; `always` parses all sources and gives each main its setup code. */
enum class MainSetupLevel { never, function, always };

extern std::list<std::string> excludelist;
extern std::list<std::string> includelist;
extern std::list<std::string> fileincludelist;
extern std::list<std::string> fileexcludelist;

// void parseInstrumentationCommand(char *line, int lineno);
bool processInstrumentationRequests(const char *fname);

/* True if `name` matches the TAU wildcard `pattern`: `star` matches zero
   or more characters, '?' matches one character, and every other
   character matches only itself. With `ignoreCase`, letters match
   either case. */
bool wildcardMatch(const std::string &pattern, const std::string &name, char star,
                   bool ignoreCase = false);

/* True if routine `name` matches an entry of `list`; '#' is the wildcard.
   Fortran names pass `ignoreCase`. */
bool routineInList(const std::list<std::string> &list, const std::string &name,
                   bool ignoreCase = false);

/* True if file `path` matches an entry of `list`; '*' is the wildcard. An
   entry with no '/' matches the basename of `path`, and an entry with a
   '/' matches the whole of `path`. */
bool fileInList(const std::list<std::string> &list, const std::string &path);

/* True if the routine lists select routine `name`: it matches no exclude
   list entry and, if the include list is not empty, an include list
   entry. Fortran names pass `ignoreCase`. */
bool instrumentRoutine(const std::string &name, bool ignoreCase = false);

/* True if the file lists select file `path`, as instrumentRoutine(). */
bool instrumentFile(const std::string &path);

/* Returns `name` with a `\` before each `\` and `"`, for a C or C++
   string literal such as the file name of a `#line` line. */
std::string escapeCString(const std::string &name);

/* Writes a `#line 1 "<source>"` line and then `source` to `output`, so
   that compiler messages, __FILE__, and debug information name the
   source. With `escape`, for C and C++, the name is escapeCString(source);
   Flang and ifx read the name of a Fortran `#line` line as written.
   Returns false after an error message that names the file. */
bool copySource(const std::string &source, const std::string &output, bool escape);

/* Warns that main program `name` still gets its setup code when the
   include list is not empty, does not match it, and the exclude list
   does not match it either. Fortran names pass `ignoreCase`. */
void warnIfMainNotIncluded(const std::string &name, bool ignoreCase = false);

#endif
