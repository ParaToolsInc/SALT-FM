# Checks the C and C++ output of saltfm for SRC, copied as
# line_escape.EXT into NAME/a"q\b/${full_timer_name}: the file name in
# the `#line` lines and in the timer names must be escaped for a C
# string literal, and the template in it must stay as written.
# NO_BACKSLASH leaves out the `\b`.
#
# With COMPILER, the output must compile, and the program must print
# the path of the copy as its last line (__FILE__), after the timer
# names that CONFIG prints, which must hold that path and each text of
# EXPECT, a list separated by `|`. With SIF, the file lists exclude the
# copy, so saltfm copies it after a `#line 1` line and the path is the
# only line. Without COMPILER, the output, with the default
# configuration file, must hold the escaped path in a timer name. STD
# is the language standard for both the parse and the compile.
#
# Usage: cmake -DSALTFM=<saltfm> -DSRC=<source> -DEXT=<c|cpp> -DNAME=<name>
#          [-DCOMPILER=<compiler> -DCONFIG=<config>] [-DSIF=<sif>]
#          [-DEXPECT=<text>[|<text>...]] [-DSTD=<standard>]
#          [-DNO_BACKSLASH=ON] -P check_line_escape.cmake

set(_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/a\"q\\b")
if(NO_BACKSLASH)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/a\"q")
endif()
string(APPEND _dir "/\${full_timer_name}")
set(_src "${_dir}/line_escape.${EXT}")
set(_out "${NAME}/line_escape.inst.${EXT}")
set(_args "")
if(SIF)
  list(APPEND _args "--tau_select_file=${SIF}")
endif()
if(CONFIG)
  list(APPEND _args "--config_file=${CONFIG}")
endif()
set(_std "")
set(_parse_std "")
if(STD)
  set(_std "-std=${STD}")
  set(_parse_std -- "${_std}")
endif()

# Runs the command in ARGN; stops unless it exits with status 0.
function(_run)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit status ${rc}: ${ARGN}")
  endif()
endfunction()
# Stops unless variable `var` holds `part`. The variable is passed by
# name, as an argument with `;` and `[` would be split.
function(_expect var part)
  string(FIND "${${var}}" "${part}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "Expected\n  ${part}\nin ${var}:\n${${var}}")
  endif()
endfunction()
# CMake's file() commands take `\` as a directory separator
file(REMOVE_RECURSE "${NAME}")
_run(mkdir -p "${_dir}")
_run(cp "${SRC}" "${_src}")
_run("${SALTFM}" ${_args} "--tau_output=${_out}" "${_src}" ${_parse_std})
if(NOT COMPILER)
  string(REPLACE "\\" "\\\\" _escaped "${_src}")
  string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
  file(READ "${_out}" _text)
  _expect(_text "TAU_PROFILE_TIMER(tautimer, \"")
  _expect(_text "[{${_escaped}} {")
  return()
endif()
_run("${COMPILER}" ${_std} -o "${NAME}/line_escape" "${_out}")
execute_process(COMMAND "./${NAME}/line_escape" OUTPUT_VARIABLE _output
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "exit status ${rc}:\n${_output}")
endif()
set(_tail "${_src}\n")
if(NOT SIF)
  _expect(_output "[{${_src}} {")
  string(REPLACE "|" ";" _expected "${EXPECT}")
  foreach(part IN LISTS _expected)
    _expect(_output "${part}")
  endforeach()
  set(_tail "\n${_tail}")
endif()
string(LENGTH "${_output}" _length)
string(LENGTH "${_tail}" _tail_length)
math(EXPR _start "${_length} - ${_tail_length}")
if(_start LESS 0)
  set(_start 0)
endif()
string(SUBSTRING "${_output}" ${_start} -1 _end)
if(NOT _end STREQUAL _tail OR (SIF AND NOT _start EQUAL 0))
  message(FATAL_ERROR "The output does not end with\n  ${_src}\n:\n${_output}")
endif()
