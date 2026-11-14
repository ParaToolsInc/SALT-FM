# Installs the build in BUILD to a fresh PREFIX and checks the MPI files
# that the Fortran parse reads (#92): the install must have SALT-FM's
# files from MPICH, but no file that could change a build that uses
# another MPI: no mpi.mod or other .mod file, no mpi.h, and no MPI
# library. The installed saltfm must then instrument SRC, which uses the
# mpi module.
#
# Usage: cmake -DBUILD=<dir> -DPREFIX=<dir> -DSRC=<source>
#          -P check_mpi_install.cmake

foreach(arg BUILD PREFIX SRC)
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "check_mpi_install.cmake needs -D${arg}=<value>")
  endif()
endforeach()

file(REMOVE_RECURSE "${PREFIX}")
execute_process(COMMAND ${CMAKE_COMMAND} --install "${BUILD}" --prefix "${PREFIX}"
  OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
set(_mpich "${PREFIX}/include/salt/mpi/mpich")
foreach(file include/mpif.h modules/mpi.saltmod COPYRIGHT)
  if(NOT EXISTS "${_mpich}/${file}")
    message(FATAL_ERROR "The install has no ${_mpich}/${file}")
  endif()
endforeach()
file(GLOB_RECURSE _found "${PREFIX}/*.mod" "${PREFIX}/*/mpi.h" "${PREFIX}/*/libmpi*"
  "${PREFIX}/*/libpmpi*" "${PREFIX}/*/libmpifort*" "${PREFIX}/*/libfmpich*")
if(_found)
  list(JOIN _found "\n  " _found)
  message(FATAL_ERROR "The install has files that can change a build:\n  ${_found}")
endif()

execute_process(COMMAND "${PREFIX}/bin/saltfm" "--tau_output=${PREFIX}/out.inst.F90" "${SRC}"
  RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "exit status ${_rc}: ${PREFIX}/bin/saltfm ${SRC}\n${_out}")
endif()
file(READ "${PREFIX}/out.inst.F90" _inst)
if(NOT _inst MATCHES "TAU_PROFILE_START")
  message(FATAL_ERROR "${PREFIX}/out.inst.F90 has no timer")
endif()
message("The install has the MPICH files and no mpi.mod, mpi.h, or MPI library")
