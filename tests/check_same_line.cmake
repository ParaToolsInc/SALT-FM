# Checks the output OUT that saltfm writes for the Fortran program SRC,
# with the configuration file CONFIG, by default the default one, when a
# `return`, an if-return, or an End statement shares a line with other
# statements, or has a label (#118). saltfm reads a copy of SRC in the
# current directory, named as OUT with the extension of SRC, so the timer
# names hold no directory. With WARNING, the standard error of saltfm
# must match the regular expression WARNING. OUT, compiled with COMPILER
# and STUB, the TAU calls that check the timers, must print what SRC
# prints. OUT must also equal EXPECTED, without the `#line` directives
# that saltfm writes (with no file, or the name of the copy) and the
# blank lines, in any case, as Flang versions differ in the case of a
# program name. Flang 19-22 write `./` before the name of the copy, in
# `#line` directives and timer names, which moves where a timer name
# splits over lines, so the comparison drops that `./` and joins each
# line that ends with `&` to a next line that starts with `     &`.
# No line of OUT but a directive may pass column WIDTH, by default 72 for
# a `.f` SRC (fixed form) and 132 otherwise. FLAGS are compiler flags
# for saltfm and COMPILER. With NO_RUN, OUT is only compiled, not run.
#
# Usage: cmake -DSALTFM=<saltfm> -DCOMPILER=<compiler> -DSTUB=<stub>
#          -DSRC=<source> -DOUT=<output> -DEXPECTED=<expected>
#          [-DCONFIG=<config>] [-DWARNING=<regex>] [-DFLAGS=<flags>]
#          [-DWIDTH=<width>] [-DNO_RUN=ON] -P check_same_line.cmake

# Runs the command in ARGN, sets `var` to its standard output and `_err`
# to its standard error, and stops unless it exits with status 0.
function(_run var)
  execute_process(COMMAND ${ARGN} OUTPUT_VARIABLE out ERROR_VARIABLE err
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit status ${rc}: ${ARGN}\n${out}${err}")
  endif()
  set(${var} "${out}" PARENT_SCOPE)
  set(_err "${err}" PARENT_SCOPE)
endfunction()

get_filename_component(_name "${OUT}" NAME_WE)
get_filename_component(_ext "${SRC}" LAST_EXT)
set(_copy "${_name}${_ext}")
configure_file("${SRC}" "${_copy}" COPYONLY)
set(_saltfm_flags)
if(CONFIG)
  set(_saltfm_flags "--config_file=${CONFIG}")
endif()
if(FLAGS)
  list(APPEND _saltfm_flags -- ${FLAGS})
endif()
_run(_ignored "${SALTFM}" "--tau_output=${OUT}" "${_copy}" ${_saltfm_flags})
if(WARNING AND NOT _err MATCHES "${WARNING}")
  message(FATAL_ERROR "saltfm does not warn as `${WARNING}`:\n${_err}")
endif()
_run(_ignored "${COMPILER}" ${FLAGS} -o "${_name}.orig" "${_copy}")
_run(_ignored "${COMPILER}" ${FLAGS} -o "${_name}.inst" "${OUT}" "${STUB}")
if(NOT NO_RUN)
  _run(_expected "./${_name}.orig")
  _run(_actual "./${_name}.inst")
  if(NOT _actual STREQUAL _expected)
    message(FATAL_ERROR
      "${SRC} prints:\n${_expected}\nbut ${OUT} prints:\n${_actual}")
  endif()
endif()

if(NOT WIDTH)
  set(WIDTH 132)
  if(_ext STREQUAL ".f")
    set(WIDTH 72)
  endif()
endif()
math(EXPR _min "${WIDTH} + 1")
# In fixed form, a tab at the start of a line stands for columns 1-6, or
# for columns 1-5 when a digit, in column 6, follows it
set(_columns "${OUT}")
if(_ext STREQUAL ".f")
  file(READ "${OUT}" _text)
  string(REGEX REPLACE "(^|\n)\t([0-9])" "\\1     \\2" _text "${_text}")
  string(REGEX REPLACE "(^|\n)\t" "\\1      " _text "${_text}")
  set(_columns "${_name}.columns")
  file(WRITE "${_columns}" "${_text}")
endif()
file(STRINGS "${_columns}" _long LENGTH_MINIMUM ${_min} REGEX "^[^#]")
if(_long)
  message(FATAL_ERROR "${OUT} has lines past column ${WIDTH}:\n${_long}")
endif()

set(_compared)
foreach(path "${OUT}" "${EXPECTED}")
  file(READ "${path}" _text)
  string(REPLACE "&\n     &" "" _text "${_text}")
  string(REPLACE "{./${_copy}}" "{${_copy}}" _text "${_text}")
  string(REPLACE "\"./${_copy}\"" "\"${_copy}\"" _text "${_text}")
  get_filename_component(_base "${path}" NAME)
  file(WRITE "${_name}.compared.${_base}" "${_text}")
  list(APPEND _compared "${_name}.compared.${_base}")
endforeach()
execute_process(COMMAND sh -c
  "grep -Ev -e '^#line [0-9]+( \"${_copy}\")?$' -e '^[[:space:]]*$' \"$1\" |
    diff -iu \"$2\" -"
  sh ${_compared} RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "${OUT} differs from ${EXPECTED}")
endif()
