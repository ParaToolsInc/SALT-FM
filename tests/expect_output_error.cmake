# Runs the command after `--` and passes when it exits with a status from
# 1 to 127, so a crash (a signal, or 128+N through a shell wrapper) fails,
# and writes an `ERROR:` line naming FILE, and holding TEXT if set, to
# stderr.
#
# Usage: cmake -DFILE=<file> [-DTEXT=<text>]
#          -P expect_output_error.cmake -- <command> [<arg>...]

set(_cmd)
set(_in_cmd FALSE)
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(idx RANGE 0 ${last} 1)
  if(_in_cmd)
    list(APPEND _cmd "${CMAKE_ARGV${idx}}")
  elseif(CMAKE_ARGV${idx} STREQUAL "--")
    set(_in_cmd TRUE)
  endif()
endforeach()

execute_process(COMMAND ${_cmd}
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
message("${out}${err}")
if(NOT rc MATCHES "^[1-9][0-9]*$" OR rc GREATER 127)
  message(FATAL_ERROR "Expected an exit status from 1 to 127, got: ${rc}")
endif()
string(REGEX MATCHALL "ERROR: [^\n]*" errors "${err}")
foreach(line IN LISTS errors)
  string(FIND "${line}" "${FILE}" pos)
  string(FIND "${line}" "${TEXT}" text_pos)
  if(pos GREATER_EQUAL 0 AND text_pos GREATER_EQUAL 0)
    return()
  endif()
endforeach()
message(FATAL_ERROR "Expected an ERROR line naming ${FILE} and holding '${TEXT}' on stderr")
