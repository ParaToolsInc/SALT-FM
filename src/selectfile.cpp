#include <cctype>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <set>

#include "selectfile.hpp"
#include "dprint.hpp"

// TODO refactor this to class instead of using global variables
// TODO modernize C++

std::list<std::string> excludelist;
std::list<std::string> includelist;
std::list<std::string> fileincludelist;
std::list<std::string> fileexcludelist;

void dump_list(std::list<std::string> l) {
  for (std::string s : l) {
    DPRINT("%s\n", s.c_str());
  }
}

/* Removes the whitespace at the two ends of `line`, including the '\r' of a
   CRLF line. */
static void trimwhitespace(std::string &line)
{
  const char *space = " \t\r\n\v\f";
  line.erase(line.find_last_not_of(space) + 1);
  line.erase(0, line.find_first_not_of(space));
}

void parseError(const char *message, char *line, int lineno, int column)
{
  fprintf(stderr,
    "ERROR: %s: parse error at selective instrumentation file line %d col %d\n",
    message, lineno, column);
  fprintf(stderr, "line=%s\n", line);
  exit(0);
}

///////////////////////////////////////////////////////////////////////////
// parseInstrumentationCommand
// input: line -  character string containing a line of text from the selective
// instrumentation file
// input: lineno - integer line no. (for reporting parse errors if any)
//
///////////////////////////////////////////////////////////////////////////
// void parseInstrumentationCommand(char *line, int lineno)
// {
//   char *original;
//   int i, ret, value;
//   bool filespecified = false;
//   bool phasespecified = false;
//   bool timerspecified = false;
//   itemQualifier_t qualifier = STATIC;
//   bool staticspecified = false;
//   bool dynamicspecified = false;
//   char pname[INBUF_SIZE]; /* parsed name */
//   char pfile[INBUF_SIZE]; /* parsed filename */
//   char plineno[INBUF_SIZE]; /* parsed lineno */
//   char pcode[INBUF_SIZE]; /* parsed code */
//   char plang[INBUF_SIZE]; /* parsed language */
//   char plevel[INBUF_SIZE]; /* parsed loop level */
//   int language = tauInstrument::LA_ANY;
//   int startlineno, stoplineno;
//   int level = 1; // Default loop instrumentation level
//
//   startlineno = stoplineno = 0;
//   instrumentKind_t kind = TAU_NOT_SPECIFIED;
//
//   DEBUG_MSG("Inside parseInstrumentationCommand: line %s lineno: %d\n", line, lineno);
//
//   original = line;
//   line = trimwhitespace(line);
//
//   /* parse: file = "foo.cc" line = 245 code = "TAU_NODE(0);" lang = "c++" */
//   if (strncmp(line, "file", 4) == 0) {
//     DEBUG_MSG("Found FILE!\n");
//
//     line += 4;
//     WSPACE(line);
//     TOKEN('=');
//     WSPACE(line);
//     TOKEN('"');
//     RETRIEVESTRING(pfile, line);
//     filespecified = true;
//     DEBUG_MSG("GOT name = %s\n", pfile);
//     WSPACE(line);
//     if (strncmp(line, "line", 4) == 0) { /* got line token, get line no. */
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       RETRIEVENUMBER(plineno, line);
//       ret = sscanf(plineno, "%d", &value);
//       DEBUG_MSG("GOT line no = %d, line = %s\n", value, line);
//     } else {
//       parseError("<line> token not found", line, lineno, line - original);
//     }
//
//     WSPACE(line);
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//
//     WSPACE(line);
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     } DEBUG_MSG("file = %s, code = %s, line no = %d, language = %d\n", pfile, pcode, value, language);
//     instrumentList.push_back(new tauInstrument(string(pfile), value, string(pcode), TAU_LINE, language));
//
//     /* parse: entry routine = "foo()" code = "TAU_SET_NODE(0)" lang = "c" */
//   } else if (strncmp(line, "entry", 5) == 0) {
//     DEBUG_MSG("Found ENTRY!\n");
//
//     line += 5;
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//       DEBUG_MSG("GOT file = %s\n", pfile);
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT routine = %s\n", pname);
//     } else {
//       strcpy(pname, "#");
//     }
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4; /* move 4 spaces */
//       /* check for = <WSPACE> " */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     }
//
//     DEBUG_MSG("entry routine = %s, code = %s, lang = %d\n", pname, pcode, language);
//
//     if (filespecified) {
//       instrumentList.push_back(
//           new tauInstrument(string(pfile), string(pname), string(pcode), TAU_ROUTINE_ENTRY, language));
//     } else {
//       instrumentList.push_back(new tauInstrument(string(pname), string(pcode), true, TAU_ROUTINE_ENTRY, language));
//     } /* file and routine are both specified for entry */
//
//     /* parse: exit routine = "foo()" code = "bar()" lang = "c" */
//   } else if (strncmp(line, "exit", 4) == 0) {
//
//     line += 4;
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//       DEBUG_MSG("GOT file = %s\n", pfile);
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT routine = %s\n", pname);
//     } else {
//       strcpy(pname, "#");
//     }
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     }
//
//     DEBUG_MSG("exit routine = %s, code = %s, lang = %d\n", pname, pcode, language);
//
//     if (filespecified) {
//       instrumentList.push_back(
//           new tauInstrument(string(pfile), string(pname), string(pcode), TAU_ROUTINE_EXIT, language));
//     } else {
//       instrumentList.push_back(new tauInstrument(string(pname), string(pcode), true, TAU_ROUTINE_EXIT, language));
//     } /* file and routine are both specified for exit */
//
//     /* parse: abort routine = "foo()" code = "bar()" lang = "c" */
//   } else if (strncmp(line, "abort", 5) == 0) {
//     DEBUG_MSG("Found ABORT!\n");
//     line += 5;
//
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//       DEBUG_MSG("GOT file = %s\n", pfile);
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT routine = %s\n", pname);
//     } else {
//       strcpy(pname, "#");
//     }
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     }
//
//     DEBUG_MSG("entry routine = %s, code = %s, lang = %d\n", pname, pcode, language);
//
//     if (filespecified) {
//       instrumentList.push_back(new tauInstrument(string(pfile), string(pname), string(pcode), TAU_ABORT, language));
//     } else {
//       instrumentList.push_back(new tauInstrument(string(pname), string(pcode), true, TAU_ABORT, language));
//     } /* file and routine are both specified for abort */
//
//     /* parse: init code = "init();" lang = "c" */
//   } else if (strncmp(line, "init", 4) == 0) {
//     DEBUG_MSG("Found INIT!\n");
//     line += 4;
//
//     WSPACE(line);
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4; /* move 4 spaces */
//       /* check for = <WSPACE> " */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     }
//
//     DEBUG_MSG("code = %s\n", pcode);
//
//     instrumentList.push_back(new tauInstrument(string(pcode), true, TAU_INIT, language));
//
//   } else if (strncmp(line, "decl", 4) == 0) {
//     DEBUG_MSG("Found DECL!\n");
//     line += 4;
//
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//
//       DEBUG_MSG("GOT file = %s\n", pfile);
//
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//
//       DEBUG_MSG("GOT routine = %s\n", pname);
//
//     } else {
//       strcpy(pname, "#");
//     }
//     if (strncmp(line, "code", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVECODE(pcode, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT code = %s\n", pcode);
//     } else {
//       parseError("<code> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "lang", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(plang, line);
//       WSPACE(line);
//       DEBUG_MSG("GOT lang = %s\n", plang);
//       language = parseLanguageString(plang);
//       if (language < 0) parseError("<lang> token invalid", line, lineno, line - original);
//     }
//
//     DEBUG_MSG("decl routine = %s, code = %s, lang = %d\n", pname, pcode, language);
//
//     if (filespecified) {
//       instrumentList.push_back(
//           new tauInstrument(string(pfile), string(pname), string(pcode), TAU_ROUTINE_DECL, language));
//     } else {
//       instrumentList.push_back(
//           new tauInstrument(string(pname), string(pcode), true, TAU_ROUTINE_DECL, language));
//     } /* file and routine are both specified for entry */
//
//   } else if (strncmp(line, "loops", 5) == 0) {
//     kind = TAU_LOOPS;
//     line += 5;
//     DEBUG_MSG("GOT loops lineno = %d\n", lineno);
//   } else if (strncmp(line, "io", 2) == 0) {
//     kind = TAU_IO;
//     line += 2;
//     DEBUG_MSG("GOT io lineno = %d\n", lineno);
//   } else if (strncmp(line, "memory", 6) == 0) {
//     kind = TAU_MEMORY;
//     line += 6;
//     DEBUG_MSG("GOT memory lineno = %d\n", lineno);
//   } else if (strncmp(line, "forall", 6) == 0) {
//     kind = TAU_FORALL;
//     line += 6;
//     DEBUG_MSG("GOT forall lineno = %d\n", lineno);
//   } else if (strncmp(line, "barrier", 7) == 0) {
//     kind = TAU_BARRIER;
//     line += 7;
//     DEBUG_MSG("GOT barrier lineno = %d\n", lineno);
//   } else if (strncmp(line, "fence", 5) == 0) {
//     kind = TAU_FENCE;
//     line += 5;
//     DEBUG_MSG("GOT fence lineno = %d\n", lineno);
//   } else if (strncmp(line, "notify", 6) == 0) {
//     kind = TAU_NOTIFY;
//     line += 6;
//     DEBUG_MSG("GOT notify lineno = %d\n", lineno);
//   } else if(strncmp(line, "phase", 5) == 0) {
//     phasespecified = true;
//     kind = TAU_PHASE;
//     line += 5;
//     DEBUG_MSG("GOT phase lineno = %d\n", lineno);
//   } else if(strncmp(line, "timer", 5) == 0) {
//     timerspecified = true;
//     kind = TAU_TIMER;
//     line += 5;
//     DEBUG_MSG("GOT timer lineno = %d\n", lineno);
//   } else if(strncmp(line, "static", 6) == 0) {
//     staticspecified = true;
//     qualifier = STATIC;
//     line += 6;
//     DEBUG_MSG("GOT static lineno = %d\n", lineno);
//   } else if(strncmp(line, "dynamic", 7) == 0) {
//     dynamicspecified = true;
//     qualifier = DYNAMIC;
//     line += 7;
//     DEBUG_MSG("GOT dynamic lineno = %d\n", lineno);
//   } else {
//     parseError("unrecognized token", line, lineno, line - original);
//   }
//
//   /* we have either static/dynamic phase/timer ... */
//   if (staticspecified || dynamicspecified) {
//     /* proceed to the next keyword */
//     WSPACE(line);
//     if (strncmp(line, "phase", 5) == 0) {
//       phasespecified = true;
//       kind = TAU_PHASE;
//       line += 5;
//       DEBUG_MSG("GOT phase command lineno = %d\n", lineno);
//     } else if (strncmp(line, "timer", 5) == 0) {
//       timerspecified = true;
//       kind = TAU_TIMER;
//       line += 5;
//       DEBUG_MSG("GOT timer command lineno = %d\n", lineno);
//     } else {
//       parseError("<phase/timer> token not found", line, lineno, line - original);
//     } /* at this stage we have static/dynamic phase/timer definition */
//   } /* static || dynamic specified */
//
//   switch (kind) {
//   case TAU_NOT_SPECIFIED:
//     break;  // Some instrumentation directives don't have a specific kind
//   case TAU_LOOPS: {
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//       DEBUG_MSG("GOT file = %s\n", pfile);
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       /* found routine */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//       DEBUG_MSG("got routine = %s\n", pname);
//     } else {
//       parseError("<routine> token not found", line, lineno, line - original);
//     }
//     if (strncmp(line, "level", 5) == 0) {
//       line += 5;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       RETRIEVENUMBERATEOL(plevel, line);
//       ret = sscanf(plevel, "%d", &level);
//       WSPACE(line);
//       DEBUG_MSG("GOT loop level = %d, line = %s\n", level, line);
//       if (level > 0) {
//         setLoopInstrumentationLevel(level);
//       } else {
//         parseError("Invalid loop level: must be greater than 0\n", line, lineno, line - original);
//       }
//     }
//     if (filespecified) {
//       instrumentList.push_back(new tauInstrument(string(pfile), string(pname), kind));
//     } else {
//       instrumentList.push_back(new tauInstrument(string(pname), kind));
//     }
//   }
//   break; // END case TAU_LOOPS
//
//   case TAU_IO:      // Fall through
//   case TAU_MEMORY:  // Fall through
//   case TAU_FORALL:  // Fall through
//   case TAU_BARRIER: // Fall through
//   case TAU_FENCE:   // Fall through
//   case TAU_NOTIFY: {
//     WSPACE(line);
//     if (strncmp(line, "file", 4) == 0) {
//       line += 4;
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pfile, line);
//       WSPACE(line);
//       filespecified = true;
//       DEBUG_MSG("GOT file = %s\n", pfile);
//     }
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       /* found routine */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       DEBUG_MSG("got routine = %s\n", pname);
//       if (filespecified) {
//         instrumentList.push_back(new tauInstrument(string(pfile), string(pname), kind));
//       } else {
//         instrumentList.push_back(new tauInstrument(string(pname), kind));
//       }
//     } else {
//       parseError("<routine> token not found", line, lineno, line - original);
//     }
//   }
//   break; // END case TAU_NOTIFY
//
//   case TAU_PHASE:    // Fall through
//   case TAU_TIMER: {
//     WSPACE(line);
//
//     /* static/dynamic phase/timer routine = "..." */
//     if (strncmp(line, "routine", 7) == 0) {
//       line += 7;
//       /* found routine */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       DEBUG_MSG("s/d p/t got routine = %s\n", pname);
//
//       /* static/dynamic phase/timer name = "..." file=<name> line = <no> to line = <no> */
//     } else if (strncmp(line, "name", 4) == 0) {
//       line += 4;
//       /* found name */
//       WSPACE(line);
//       TOKEN('=');
//       WSPACE(line);
//       TOKEN('"');
//       RETRIEVESTRING(pname, line);
//       WSPACE(line);
//       DEBUG_MSG("s/d p/t got name = %s\n", pname);
//
//       /* name was parsed. Look for line = <no> to line = <no> next */
//       if (strncmp(line, "file", 4) == 0) { /* got line token, get line no. */
//         line += 4;
//         WSPACE(line);
//         TOKEN('=');
//         WSPACE(line);
//         TOKEN('"');
//         RETRIEVESTRING(pfile, line);
//         filespecified = true;
//         WSPACE(line);
//         DEBUG_MSG("GOT file = %s\n", pfile);
//       } else {
//         parseError("<file> token not found", line, lineno, line - original);
//       }
//       if (strncmp(line, "line", 4) == 0) { /* got line token, get line no. */
//         line += 4;
//         WSPACE(line);
//         TOKEN('=');
//         WSPACE(line);
//         RETRIEVENUMBER(plineno, line);
//         ret = sscanf(plineno, "%d", &startlineno);
//         DEBUG_MSG("GOT start line no = %d, line = %s\n", startlineno, line);
//         WSPACE(line);
//         if (strncmp(line, "to", 2) == 0) {
//           line += 2;
//           WSPACE(line);
//           /* look for line=<no> next */
//           if (strncmp(line, "line", 4) == 0) {
//             line += 4;
//             WSPACE(line);
//             TOKEN('=');
//             WSPACE(line);
//             RETRIEVENUMBERATEOL(pcode, line);
//             ret = sscanf(pcode, "%d", &stoplineno);
//             DEBUG_MSG("GOT stop line no = %d\n", stoplineno);
//           } else { /* we got line=<no> to , but there was no line */
//             parseError("<line> token not found in the stop declaration", line, lineno, line - original);
//           } /* parsed to clause */
//         } else { /* hey, line = <no> is there, but there is no "to" */
//           parseError("<to> token not found", line, lineno, line - original);
//         } /* we have parsed all the tokens now. Let us see what was specified phase/timer routine = <name> or phase/timer name =<name> line = <no> to line = <no> */
//       } else {
//         parseError("<line> token not found in the start declaration", line, lineno, line - original);
//       } /* line specified */
//     } else { /* name or routine not specified */
//       parseError("<routine/name> token not found", line, lineno, line - original);
//     } /* end of routine/name processing */
//
//     /* create instrumentation requests */
//     if (filespecified) { /* [static/dynamic] <phase/timer> name = "<name>" file="<name>" line=a to line=b */
//       instrumentList.push_back(new tauInstrument(qualifier, kind, pname, pfile, startlineno, stoplineno));
//     } else { /* [static/dynamic] <phase/timer> routine = "<name>" */
//       instrumentList.push_back(new tauInstrument(qualifier, kind, pname));
//     }
//   }
//   break; // END case TAU_TIMER
//
//   default: {
//     parseError("Internal error: unknown instrumentKind_t", line, lineno, line - original);
//   }
//   break;
//   } // END switch (kind)
//
// } // END void parseInstrumentationCommand(char *line, int lineno)

/* True if the '#' comment line could be an unquoted leading-'#' pattern.
   Errs toward warning: only end of line, whitespace and these punctuation
   characters stay quiet, since they make comment decoration or rulers
   ('#=====', '#////', '#|') and are implausible at the start of a routine
   pattern: " ' + , / ; = ? \ ^ ` { | }. Anything else, including
   letters, digits, other punctuation and bytes >= 0x80, warns. */
static bool looksLikeUnquotedPattern(const char *line)
{
  const unsigned char c = static_cast<unsigned char>(line[1]);
  return c != '\0' && !isspace(c) && strchr("\"'+,/;=?\\^`{|}", c) == nullptr;
}

/* A leading '#' starts a comment even inside list blocks (TAU/PDT syntax).
   In routine lists, where '#' is also the wildcard, warn when the comment
   looks like an unquoted leading-'#' pattern. */
static void warnIfUnquotedWildcard(const char *line, const char *listname, int lineno)
{
  if (looksLikeUnquotedPattern(line)) {
    fprintf(stderr,
      "WARNING: '%s' at selective instrumentation file line %d treated as a comment in %s. Quote it (\"%s\") to use it as a wildcard\n",
      line, lineno, listname, line);
  }
}

/* Strip optional surrounding quotes from a list-block entry. Quoting is how
   a pattern starts with the '#' wildcard, since a bare leading '#' starts a
   comment; the closing quote is optional for TAU compatibility. Warns about
   quoting that silently changes or ignores part of the entry. */
static std::string parseListEntry(const char *entry, const char *listname, int lineno)
{
  std::string result(entry);
  if (result[0] != '"') {
    return result;
  }
  const size_t close = result.find('"', 1);
  const bool closed = close != std::string::npos;
  if (closed && result.find_first_not_of(" \t\r\n\v\f", close + 1) != std::string::npos) {
    fprintf(stderr,
      "WARNING: ignoring text after closing quote in %s entry at selective instrumentation file line %d: %s\n",
      listname, lineno, result.c_str() + close + 1);
  }
  result = closed ? result.substr(1, close - 1) : result.substr(1);
  /* A lone '"' gets only the empty-pattern warning. */
  if (result.empty()) {
    fprintf(stderr,
      "WARNING: empty quoted %s entry at selective instrumentation file line %d matches nothing\n",
      listname, lineno);
  } else if (!closed) {
    fprintf(stderr,
      "WARNING: missing closing quote in %s entry at selective instrumentation file line %d; using '%s'\n",
      listname, lineno, result.c_str());
  }
  return result;
}

/* Reads the next line of `input` into `line`, with no length limit, and
   trims it. Returns false at the end of the file. */
static bool readLine(std::istream &input, std::string &line, int &lineno)
{
  if (!std::getline(input, line)) {
    return false;
  }
  lineno++;
  trimwhitespace(line);
  return true;
}

/* Reads the entries of a list up to its `end` token into `list`. A leading
   '#' starts a comment; routine lists warn about comments that look like
   patterns. Routine lists remove quotes; file lists keep them, as TAU
   does, and warn. */
static void readList(std::istream &input, int &lineno, const char *end,
                     std::list<std::string> &list, const char *listname, bool routines)
{
  std::string line;
  while (readLine(input, line, lineno) && line != end) {
    if (line.empty()) {
      continue;
    }
    if (line[0] == '#') {
      if (routines) {
        warnIfUnquotedWildcard(line.c_str(), listname, lineno);
      }
      continue;
    }
    if (routines) {
      list.push_back(parseListEntry(line.c_str(), listname, lineno));
    } else {
      if (line[0] == '"') {
        fprintf(stderr,
          "WARNING: %s entry at selective instrumentation file line %d:\n"
          "  the quotes in %s are part of the pattern; remove them to match\n"
          "  file names without quotes\n",
          listname, lineno, line.c_str());
      }
      list.push_back(line);
    }
    DPRINT("Adding %s to %s\n", list.back().c_str(), listname);
  }
}

bool processInstrumentationRequests(const char *fname)
{

  std::ifstream input(fname);
  std::string line;
  int lineno = 0;


  if (!input) {
    std::cerr << "ERROR: Cannot open selective instrumentation file: " << fname << std::endl;
    return false;
  }


  DPRINT0("Inside processInstrumentationRequests\n");
  while (readLine(input, line, lineno)) {
    if (line == BEGIN_EXCLUDE_TOKEN) {
      readList(input, lineno, END_EXCLUDE_TOKEN, excludelist, "exclude list", true);
    } else if (line == BEGIN_INCLUDE_TOKEN) {
      readList(input, lineno, END_INCLUDE_TOKEN, includelist, "include list", true);
    } else if (line == BEGIN_FILE_INCLUDE_TOKEN) {
      readList(input, lineno, END_FILE_INCLUDE_TOKEN, fileincludelist, "file include list", false);
    } else if (line == BEGIN_FILE_EXCLUDE_TOKEN) {
      readList(input, lineno, END_FILE_EXCLUDE_TOKEN, fileexcludelist, "file exclude list", false);
    } else if (line == BEGIN_INSTRUMENT_SECTION) {
      fprintf(stderr, "WARNING: Instrument section is not supported yet.\n");
      while (readLine(input, line, lineno) && line != END_INSTRUMENT_SECTION) {
        // parseInstrumentationCommand(line, lineno);
      }
    }
    /* Comments, blank lines and other lines outside a list are ignored. */
  }
  input.close();

  DPRINT0("includelist\n");
  dump_list(includelist);

  DPRINT0("excludelist\n");
  dump_list(excludelist);

  DPRINT0("fileincludelist\n");
  dump_list(fileincludelist);

  DPRINT0("fileexcludelist\n");
  dump_list(fileexcludelist);

  return true;
}

bool wildcardMatch(const std::string &pattern, const std::string &name, char star,
                   bool ignoreCase)
{
  const auto same = [ignoreCase](char a, char b) {
    return a == b || (ignoreCase && tolower(static_cast<unsigned char>(a)) ==
                                        tolower(static_cast<unsigned char>(b)));
  };
  /* On a mismatch, retry from the last `star` with one more character
     matched by it. */
  size_t p = 0;
  size_t n = 0;
  size_t star_p = std::string::npos;
  size_t star_n = 0;
  while (n < name.size()) {
    if (p < pattern.size() && pattern[p] == star) {
      star_p = p++;
      star_n = n;
    } else if (p < pattern.size() && (pattern[p] == '?' || same(pattern[p], name[n]))) {
      p++;
      n++;
    } else if (star_p != std::string::npos) {
      p = star_p + 1;
      n = ++star_n;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == star) {
    p++;
  }
  return p == pattern.size();
}

bool routineInList(const std::list<std::string> &list, const std::string &name,
                   bool ignoreCase)
{
  for (const std::string &entry : list) {
    if (wildcardMatch(entry, name, '#', ignoreCase)) {
      return true;
    }
  }
  return false;
}

bool fileInList(const std::list<std::string> &list, const std::string &path)
{
  const std::string basename = path.substr(path.find_last_of('/') + 1);
  for (const std::string &entry : list) {
    const bool whole = entry.find('/') != std::string::npos;
    if (wildcardMatch(entry, whole ? path : basename, '*')) {
      return true;
    }
  }
  return false;
}

bool instrumentRoutine(const std::string &name, bool ignoreCase)
{
  return !routineInList(excludelist, name, ignoreCase) &&
         (includelist.empty() || routineInList(includelist, name, ignoreCase));
}

bool instrumentFile(const std::string &path)
{
  return !fileInList(fileexcludelist, path) &&
         (fileincludelist.empty() || fileInList(fileincludelist, path));
}

void warnIfMainNotIncluded(const std::string &name, bool ignoreCase)
{
  if (!includelist.empty() && !routineInList(includelist, name, ignoreCase) &&
      !routineInList(excludelist, name, ignoreCase)) {
    fprintf(stderr,
      "WARNING: main program \"%s\" is not on the selective\n"
      "  instrumentation include list; SALT-FM still inserts its setup code\n"
      "  there, without a timer\n",
      name.c_str());
  }
}

/* Prints an error naming `file` and the reason when `failed`. */
static bool ioFailed(bool failed, const char *what, const std::string &file)
{
  if (failed) {
    fprintf(stderr, "ERROR: Could not %s %s: %s\n", what, file.c_str(), strerror(errno));
  }
  return failed;
}

std::string ReplacePhrase(std::string str, const std::string &phrase,
                          const std::string &to_replace)
{
  for (auto pos = str.find(phrase); pos != std::string::npos;
       pos = str.find(phrase, pos + to_replace.length())) {
    str.replace(pos, phrase.length(), to_replace);
  }
  return str;
}

bool copySource(const std::string &source, const std::string &output,
                const std::string &line_name)
{
  /* Open the input first, so a missing one leaves no output file */
  std::ifstream in(source);
  if (ioFailed(!in.is_open(), "open input file", source)) {
    return false;
  }
  std::ofstream out(output);
  if (ioFailed(!out.is_open(), "open output file", output)) {
    return false;
  }
  out << "#line 1 \"" << line_name << "\"\n";
  /* Copying an empty rdbuf() sets failbit, so copy only a non-empty file */
  if (in.peek() != std::ifstream::traits_type::eof()) {
    out << in.rdbuf();
  }
  out.close();
  return !ioFailed(!out, "write output file", output);
}
