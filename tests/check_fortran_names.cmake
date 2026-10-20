# Checks the Fortran output of saltfm for SRC, copied into a directory
# under NAME.
#
# MODE `line`: the copy is a\b/line_escape.f90, and the output must
# start with `#line 1 "<path>"`, the path as written, since Flang and
# ifx read the name of a Fortran `#line` line as written (#97).
#
# Usage: cmake -DSALTFM=<saltfm> -DSRC=<source> -DNAME=<name>
#          -DMODE=line [-DSIF=<sif>] -P check_fortran_names.cmake

set(_base "${CMAKE_CURRENT_BINARY_DIR}/${NAME}")
set(_dir "${_base}/a\\b")
set(_file line_escape.f90)
set(_src "${_dir}/${_file}")
set(_out "${NAME}/line_escape.inst.F90")
set(_args "")
if(SIF)
  list(APPEND _args "--tau_select_file=${SIF}")
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
_run("${SALTFM}" ${_args} "--tau_output=${_out}" "${_src}")
file(READ "${_out}" _text)
string(FIND "${_text}" "\n" _end)
string(SUBSTRING "${_text}" 0 ${_end} _first)
if(NOT _first STREQUAL "#line 1 \"${_src}\"")
  message(FATAL_ERROR "The first line is not\n  #line 1 \"${_src}\"\n:\n${_text}")
endif()
