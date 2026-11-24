# Runs cmake/modules/SALTMPIFiles.cmake on the fake MPICH source tree
# FAKE, in a fresh directory DIR, with `false` as the Flang driver, and
# checks that it goes on after a failure. The script prints the output
# of SALTMPIFiles.cmake with each warning on one line, and then
# `stamp: <file>` when SALTMPIFiles.cmake wrote its stamp file, and
# `out: <dir>` when its output directory exists. FORM is one of:
#   configure  configure fails
#   compile    configure works; each compile fails
#   again      compile, then `second run:` and compile again
#   again_cc   the same, the second time with another C compiler
#   again_cc_version  the same, with the same C compiler path and
#              another version
#
# Usage: cmake -DFORM=<form> -DFAKE=<dir> -DDIR=<dir>
#          -P check_salt_mpi_files.cmake

foreach(arg FORM FAKE DIR)
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "check_salt_mpi_files.cmake needs -D${arg}=<value>")
  endif()
endforeach()
if(FORM STREQUAL "configure")
  set(_cc false)
elseif(FORM MATCHES "^(compile|again|again_cc|again_cc_version)$")
  set(_cc cc)
else()
  message(FATAL_ERROR "Unknown FORM ${FORM}")
endif()

file(REMOVE_RECURSE "${DIR}")
set(_stamp "${DIR}/files.stamp")
set(_runs "${_cc}")
if(FORM STREQUAL "again")
  list(APPEND _runs cc)
elseif(FORM STREQUAL "again_cc")
  list(APPEND _runs c99)
elseif(FORM STREQUAL "again_cc_version")
  set(_runs "${DIR}/cc" "${DIR}/cc")
endif()
foreach(cc IN LISTS _runs)
  if(DEFINED _out)
    message("second run:")
  endif()
  if(FORM STREQUAL "again_cc_version")
    string(APPEND _version 1)
    file(WRITE "${cc}" "#!/bin/sh\necho cc version ${_version}\n")
    execute_process(COMMAND chmod +x "${cc}" COMMAND_ERROR_IS_FATAL ANY)
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} -DSRC=${FAKE} -DWORK=${DIR}/work -DOUT=${DIR}/out
      -DFC=false -DCC=${cc} -DPYTHON=python3 -DSUFFIX=.saltmod -DSTAMP=${_stamp}
      -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/modules/SALTMPIFiles.cmake
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
  # CMake wraps the text of a warning over lines
  string(REGEX REPLACE "\n  +" " " _out "${_out}")
  message("${_out}")
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "SALTMPIFiles.cmake failed with exit status ${_rc}")
  endif()
endforeach()
if(EXISTS "${_stamp}")
  message("stamp: ${_stamp}")
endif()
if(EXISTS "${DIR}/out")
  message("out: ${DIR}/out")
endif()
