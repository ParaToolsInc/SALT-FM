# Checks that each block that saltfm inserts keeps the file and line
# that the `#line` directives of SRC give the lines after it: the
# preprocessor `COMPILER -E` must report each `#warning` line of SRC at
# the same file, line, and column in the output OUT, which saltfm writes
# with CONFIG, as in SRC. OUT must also compile.
#
# Usage: cmake -DSALTFM=<saltfm> -DCONFIG=<config> -DCOMPILER=<compiler>
#          -DSRC=<source> -DOUT=<output> -P check_line_map.cmake

# Sets `var` to the list of warnings that `COMPILER -E` reports for
# `file`, each with its position.
function(_warnings file var)
  execute_process(COMMAND "${COMPILER}" -E -o /dev/null "${file}"
    ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit status ${rc}: ${COMPILER} -E ${file}\n${err}")
  endif()
  string(REGEX MATCHALL "[^\n]*: warning: [^\n]*" found "${err}")
  set(${var} "${found}" PARENT_SCOPE)
endfunction()

execute_process(COMMAND "${SALTFM}" "--config_file=${CONFIG}"
  "--tau_output=${OUT}" "${SRC}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "exit status ${_rc}: saltfm ${SRC}")
endif()
_warnings("${SRC}" _in)
_warnings("${OUT}" _out)
list(JOIN _in "\n" _in_text)
list(JOIN _out "\n" _out_text)
# A compiler that reported no warnings would pass the comparison
file(STRINGS "${SRC}" _lines REGEX "^#warning ")
list(LENGTH _lines _expected)
list(LENGTH _in _found)
if(NOT _found EQUAL _expected)
  message(FATAL_ERROR
    "${_found} warnings, not ${_expected}, for ${SRC}:\n${_in_text}")
endif()
if(NOT _out_text STREQUAL _in_text)
  message(FATAL_ERROR
    "Warnings for ${SRC}:\n${_in_text}\nbut for ${OUT}:\n${_out_text}")
endif()
execute_process(COMMAND "${COMPILER}" -fsyntax-only "${OUT}"
  ERROR_VARIABLE _err RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR
    "exit status ${_rc}: ${COMPILER} -fsyntax-only ${OUT}\n${_err}")
endif()
