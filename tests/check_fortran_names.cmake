# Checks the Fortran output of saltfm for SRC, copied into a directory
# under NAME.
#
# MODE `line`: the copy is a\b/line_escape.f90, and the output must
# start with `#line 1 "<path>"`, the path as written, since Flang and
# ifx read the name of a Fortran `#line` line as written (#97).
#
# MODE `timer`: the copy is <dir>/line_escape.f90, where <dir> holds a
# `"` placed so that in the timer name of its one-letter program, where
# the `"` is doubled for the Fortran string literal, the pair reaches the
# end of a 64-character piece of the split name, and then a `\`. The
# output must keep the pair together and its timer literal must hold the
# path, `"` doubled and `\` as written.
#
# MODE `dollar`: the copy is <dir>/${full_timer_name}/line_escape.f90,
# where <dir> holds `$&` and `$1`, which a regex replacement would
# expand. The timer literal must hold the path as written.
#
# MODE `implicit`: the copy, which holds a main program with no program
# statement, is <PAD x's>STEM.f90. Its timer literal must hold
# `<stem>._implicit_main_ [{<path>} {`, `"` doubled. A PAD of 63 and a
# STEM that starts with `"` put the pair at the end of the first piece,
# which must keep it together.
#
# With COMPILER, the outputs of the other modes must also compile, and
# the run must print the timer name (with CONFIG) as written: in
# `implicit` mode it must start the output.
#
# Usage: cmake -DSALTFM=<saltfm> -DSRC=<source> -DNAME=<name>
#          -DMODE=<line|timer|dollar|implicit>
#          [-DSTEM=<stem> [-DPAD=<count>]] [-DSIF=<sif>] [-DCONFIG=<config>] [-DCOMPILER=<compiler>]
#          -P check_fortran_names.cmake

set(_base "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
set(_dir "${_base}/a\\b")
set(_file line_escape.f90)
if(MODE STREQUAL "timer")
  # The `"` at index 63, modulo 64, of the timer name
  string(LENGTH "p [{${_base}/" _length)
  math(EXPR _pad "(127 - ${_length} % 64) % 64")
  string(REPEAT "x" ${_pad} _x)
  set(_dir "${_base}/${_x}\"q\\b")
elseif(MODE STREQUAL "dollar")
  set(_dir "${_base}/d$&e$1/\${full_timer_name}")
elseif(MODE STREQUAL "implicit")
  set(_x "")
  if(PAD)
    string(REPEAT "x" ${PAD} _x)
  endif()
  set(_stem "${_x}${STEM}")
  set(_dir "${_base}")
  set(_file "${_stem}.f90")
endif()
set(_src "${_dir}/${_file}")
set(_out "${NAME}/line_escape.inst.F90")
set(_args "")
if(SIF)
  list(APPEND _args "--tau_select_file=${SIF}")
endif()
if(CONFIG)
  list(APPEND _args "--config_file=${CONFIG}")
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
_run("${SALTFM}" ${_args} "--tau_output=${_out}" "${_src}")
file(READ "${_out}" _text)
if(MODE STREQUAL "line")
  string(FIND "${_text}" "\n" _end)
  string(SUBSTRING "${_text}" 0 ${_end} _first)
  if(NOT _first STREQUAL "#line 1 \"${_src}\"")
    message(FATAL_ERROR "The first line is not\n  #line 1 \"${_src}\"\n:\n${_text}")
  endif()
  return()
endif()

if(MODE STREQUAL "timer" OR PAD)
  # The pair at the end of a piece starts the next one
  _expect(_text "&\n     &\"\"")
endif()
# The timer literal, joined
string(REPLACE "&\n     &" "" _joined "${_text}")
set(_name " [{${_src}} {")
if(MODE STREQUAL "implicit")
  set(_name "${_stem}._implicit_main_${_name}")
endif()
string(REPLACE "\"" "\"\"" _literal "${_name}")
_expect(_joined "${_literal}")
if(NOT COMPILER)
  return()
endif()
_run("${COMPILER}" -o "${NAME}/line_escape" "${_out}")
execute_process(COMMAND "./${NAME}/line_escape" OUTPUT_VARIABLE _output
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "exit status ${rc}:\n${_output}")
endif()
string(FIND "${_output}" "${_name}" _pos)
if(_pos EQUAL -1 OR (MODE STREQUAL "implicit" AND NOT _pos EQUAL 0))
  message(FATAL_ERROR "Expected\n  ${_name}\nin the output:\n${_output}")
endif()
