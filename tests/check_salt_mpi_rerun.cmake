# Touches FILE, a compiler that the rule of the MPICH files runs, and
# builds the target of that rule, salt-mpich-files, in build tree BUILD.
# The build prints `Making the MPICH files` only when the rule runs.
#
# Usage: cmake -DBUILD=<dir> -DFILE=<file> -P check_salt_mpi_rerun.cmake

foreach(arg BUILD FILE)
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "check_salt_mpi_rerun.cmake needs -D${arg}=<value>")
  endif()
endforeach()
file(TOUCH "${FILE}")
execute_process(
  COMMAND ${CMAKE_COMMAND} --build "${BUILD}" --target salt-mpich-files
  COMMAND_ERROR_IS_FATAL ANY)
