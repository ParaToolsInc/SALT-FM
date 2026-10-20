# Passes when saltfm's output for SRC, copied as line_escape.EXT into
# NAME/a"q\b, compiles with COMPILER and the program prints the path of
# that copy as __FILE__: the `#line` lines escape the file name for C
# and C++. With SIF, the file lists exclude the copy, so saltfm copies
# it after a `#line 1` line. NO_BACKSLASH leaves out the `\b`.
#
# Usage: cmake -DSALTFM=<saltfm> -DCONFIG=<config> -DCOMPILER=<compiler>
#          -DSRC=<source> -DEXT=<c|cpp> -DNAME=<name> [-DSIF=<sif>]
#          [-DNO_BACKSLASH=ON] -P check_line_escape.cmake

set(_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/a\"q\\b")
if(NO_BACKSLASH)
  set(_dir "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/a\"q")
endif()
set(_src "${_dir}/line_escape.${EXT}")
set(_out "${NAME}/line_escape.inst.${EXT}")
set(_sif_arg "")
if(SIF)
  set(_sif_arg "--tau_select_file=${SIF}")
endif()

# Runs the command in ARGN; stops unless it exits with status 0.
function(_run)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "exit status ${rc}: ${ARGN}")
  endif()
endfunction()
# CMake's file() commands take `\` as a directory separator
file(REMOVE_RECURSE "${NAME}")
_run(mkdir -p "${_dir}")
_run(cp "${SRC}" "${_src}")
_run("${SALTFM}" "--config_file=${CONFIG}" ${_sif_arg} "--tau_output=${_out}"
  "${_src}")
_run("${COMPILER}" -o "${NAME}/line_escape" "${_out}")
execute_process(COMMAND "./${NAME}/line_escape" OUTPUT_VARIABLE file
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT file STREQUAL _src)
  message(FATAL_ERROR "__FILE__ is\n  ${file}\nnot\n  ${_src}")
endif()
