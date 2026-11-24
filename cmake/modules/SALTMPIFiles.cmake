# SALTMPIFiles.cmake -- Makes the MPICH files that the Fortran parse
# reads (#92), at build time.
#
# Runs MPICH's configure from source tree SRC in a fresh WORK directory,
# with Flang driver FC and C compiler CC. configure writes mpif.h and,
# with PYTHON, the sources of the Fortran modules. FC then compiles the
# sources of `mpi` and `mpi_f08`, with the modules that they use, and
# the module files go to OUT/modules with the suffix SUFFIX. mpif.h goes
# to OUT/include, and MPICH's license to OUT. Then the script writes
# STAMP. When configure or a compile fails, the script warns, and OUT
# lacks the files that failed; without the `mpi` module, there is no
# OUT. Flang 19 fails a Fortran 2008 check of MPICH's configure, which
# then makes no `mpi_f08` sources; the script then warns.
#
# STAMP holds a hash of the inputs: SRC's configure, FC and CC and their
# versions, SUFFIX, and these scripts. When the files in OUT (unless the
# `mpi` module failed) and WORK/mods are there and STAMP holds the same
# hash, the script only touches STAMP, so a CI cache of these files and
# STAMP skips MPICH's configure.
#
# Usage: cmake -DSRC=<dir> -DWORK=<dir> -DOUT=<dir> -DFC=<flang>
#          -DCC=<cc> -DPYTHON=<python3> -DSUFFIX=<suffix> -DSTAMP=<file>
#          -P SALTMPIFiles.cmake

foreach(arg SRC WORK OUT FC CC PYTHON SUFFIX STAMP)
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "SALTMPIFiles.cmake needs -D${arg}=<value>")
  endif()
endforeach()
include(${CMAKE_CURRENT_LIST_DIR}/SALTMPI.cmake)

execute_process(COMMAND "${FC}" --version OUTPUT_VARIABLE _key ERROR_QUIET)
execute_process(COMMAND "${CC}" --version OUTPUT_VARIABLE _cc ERROR_QUIET)
foreach(file "${SRC}/configure" "${CMAKE_CURRENT_LIST_DIR}/SALTMPI.cmake"
    "${CMAKE_CURRENT_LIST_FILE}")
  file(SHA256 "${file}" _hash)
  string(APPEND _key ";${_hash}")
endforeach()
string(SHA256 _key "${_key};${_cc};${FC};${CC};${SUFFIX}")
set(_stamped "")
if(EXISTS "${STAMP}")
  file(READ "${STAMP}" _stamped)
endif()
if(_stamped STREQUAL _key AND IS_DIRECTORY "${WORK}/mods"
    AND (EXISTS "${OUT}/include/mpif.h" OR NOT EXISTS "${WORK}/mods/mpi.mod"))
  message(STATUS "The MPICH files in ${OUT} are up to date")
  file(TOUCH "${STAMP}")
  return()
endif()

file(REMOVE_RECURSE "${WORK}" "${OUT}" "${STAMP}")
file(MAKE_DIRECTORY "${WORK}/mods")
set(_log "${WORK}/configure.log")
message(STATUS "Running MPICH configure, which can take many minutes; its log is ${_log}")
execute_process(
  COMMAND sh "${SRC}/configure" "CC=${CC}" "FC=${FC}" "F77=${FC}" "PYTHON=${PYTHON}"
    ${SALT_MPICH_CONFIGURE_ARGS}
  WORKING_DIRECTORY "${WORK}" OUTPUT_FILE "${_log}" ERROR_FILE "${_log}"
  RESULT_VARIABLE _rc)
set(_fortran "${WORK}/src/binding/fortran")
if(NOT _rc EQUAL 0 OR NOT EXISTS "${_fortran}/mpif_h/mpif.h")
  message(WARNING
    "MPICH configure failed with exit status ${_rc}, so SALT-FM has no MPICH "
    "files. See ${_log}.")
  file(TOUCH "${STAMP}")
  return()
endif()
file(COPY "${_fortran}/mpif_h/mpif.h" DESTINATION "${OUT}/include")
file(COPY "${SRC}/COPYRIGHT" DESTINATION "${OUT}")

# MPICH's build makes mpifnoext.h, which the `mpi` module sources include,
# from mpif.h with `sed -e 's/^C/\!/g' -e '/EXTERNAL/d' -e '/PMPI_/d'`
file(READ "${_fortran}/mpif_h/mpif.h" _text)
string(REGEX REPLACE "\nC" "\n!" _text "\n${_text}")
string(REGEX REPLACE "\n[^\n]*(EXTERNAL|PMPI_)[^\n]*" "" _text "${_text}")
string(SUBSTRING "${_text}" 1 -1 _text)
file(WRITE "${_fortran}/use_mpi/mpifnoext.h" "${_text}")

if(NOT EXISTS "${_fortran}/use_mpi_f08/mpi_f08_compile_constants.f90")
  message(WARNING
    "MPICH configure turned off the mpi_f08 module for ${FC}, so SALT-FM "
    "has no mpi_f08 module. See ${_log}.")
  set(SALT_MPICH_MPI_F08_SOURCES "")
endif()
# The Flang driver finds its intrinsic modules on each LLVM version, as
# `flang -fc1 -module-suffix` does not for `use, intrinsic` before LLVM
# 23. The module files differ only in name from the SUFFIX files, and
# need the intrinsic modules that SALT-FM copies from this Flang.
foreach(group mpi mpi_f08)
  string(TOUPPER "${group}" upper)
  foreach(source IN LISTS SALT_MPICH_${upper}_SOURCES)
    # configure writes some sources into the build tree; the others are
    # in the source tree
    set(_path "${_fortran}/${source}")
    if(NOT EXISTS "${_path}")
      set(_path "${SRC}/src/binding/fortran/${source}")
    endif()
    execute_process(
      COMMAND "${FC}" -fsyntax-only "-J${WORK}/mods" "-I${_fortran}/use_mpi" "${_path}"
      RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
    if(NOT _rc EQUAL 0)
      message(WARNING
        "${FC} could not compile ${_path}, so SALT-FM has no ${group} module:\n${_out}")
      break()
    endif()
  endforeach()
endforeach()
file(GLOB _mods "${WORK}/mods/*.mod")
foreach(mod IN LISTS _mods)
  cmake_path(GET mod STEM name)
  configure_file("${mod}" "${OUT}/modules/${name}${SUFFIX}" COPYONLY)
endforeach()
# The parse reads none of the files without the `mpi` module
if(NOT EXISTS "${WORK}/mods/mpi.mod")
  file(REMOVE_RECURSE "${OUT}")
endif()
file(WRITE "${STAMP}" "${_key}")
