# SALTMPI.cmake -- The MPI files that the Fortran parse reads (#92).
#
# Flang cannot read the `mpi.mod` files of an MPI that another compiler
# built, so SALT-FM makes its own from the MPI sources: the `mpi` and
# `mpi_f08` modules, the modules that they use, and `mpif.h`. This file
# records the pinned release of each MPI implementation, and finds its
# source tree. SALTMPIFiles.cmake makes the files.

# MPICH, from https://www.mpich.org/downloads/; the SHA-256 is the one
# that the GitHub release of pmodels/mpich gives
set(SALT_MPICH_VERSION 5.0.2)
set(SALT_MPICH_URL
  "https://www.mpich.org/static/downloads/${SALT_MPICH_VERSION}/mpich-${SALT_MPICH_VERSION}.tar.gz")
set(SALT_MPICH_SHA256 928c2f18d350a91443fe8024ad01ce2c6009e9c551e0a73e797fc567f119137d)

# The MPICH configure options that make mpif.h and the sources of the
# Fortran modules; SALT-FM builds no MPICH library. Without hwloc,
# configure adds no `-framework` flags on macOS, which Flang refuses.
set(SALT_MPICH_CONFIGURE_ARGS
  --with-device=ch3:nemesis --with-pm=no --with-datatype-engine=dataloop
  --with-hwloc=no --disable-romio)

# The sources of the modules `mpi` and `mpi_f08`, under
# src/binding/fortran of the MPICH source or build tree, each after the
# modules that it uses
set(SALT_MPICH_MPI_SOURCES
  use_mpi/mpi_constants.f90 use_mpi/mpi_sizeofs.f90 use_mpi/mpi_base.f90
  use_mpi/pmpi_base.f90 use_mpi/mpi.f90)
set(SALT_MPICH_MPI_F08_SOURCES
  use_mpi_f08/mpi_c_interface_types.f90 use_mpi_f08/mpi_f08_types.f90
  use_mpi_f08/mpi_f08_compile_constants.f90 use_mpi_f08/mpi_f08_callbacks.f90
  use_mpi_f08/mpi_f08_link_constants.f90 use_mpi_f08/pmpi_f08.f90
  use_mpi_f08/mpi_f08.f90)

# salt_mpi_source(<var> <url> <sha256> <dir> <out>)
#
# Sets <out> to the MPI source tree that cache variable <var> gives:
#   download          the tarball at <url>, downloaded into <dir>
#   <tarball>         a local tarball
#   <directory>       a source tree, which has `configure`
#   OFF               none; <out> is empty
# The function unpacks a tarball into <dir>/src. A tarball whose SHA-256
# is not <sha256> gets a warning. When the download fails, the function
# warns and sets <out> to empty. Any other value stops with an error, as
# salt_mpi_check_source() gives.
function(salt_mpi_source var url sha256 dir out)
  set(${out} "" PARENT_SCOPE)
  salt_mpi_check_source(${var})
  set(value "${${var}}")
  if(NOT value)
    return()
  endif()
  if(value STREQUAL "download")
    cmake_path(GET url FILENAME name)
    set(tarball "${dir}/${name}")
    _salt_mpi_download(${var} "${url}" ${sha256} "${tarball}" ok)
    if(NOT ok)
      return()
    endif()
    set(have ${sha256})
  elseif(IS_DIRECTORY "${value}")
    set(${out} "${value}" PARENT_SCOPE)
    return()
  else()
    set(tarball "${value}")
    file(SHA256 "${tarball}" have)
    if(NOT have STREQUAL sha256)
      message(WARNING
        "${var}=${value} has the SHA-256 ${have}, not ${sha256} of the "
        "release that SALT-FM pins. SALT-FM uses it anyway.")
    endif()
  endif()
  # Unpack again only for another tarball
  set(src "${dir}/src")
  set(stamp "${dir}/src.sha256")
  set(unpacked "")
  if(EXISTS "${stamp}")
    file(READ "${stamp}" unpacked)
  endif()
  if(NOT unpacked STREQUAL have)
    message(STATUS "Unpacking ${tarball}")
    file(REMOVE_RECURSE "${src}" "${stamp}")
    file(MAKE_DIRECTORY "${src}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${tarball}"
      WORKING_DIRECTORY "${src}" COMMAND_ERROR_IS_FATAL ANY)
    file(WRITE "${stamp}" "${have}")
  endif()
  file(GLOB configure LIST_DIRECTORIES false "${src}/*/configure")
  list(LENGTH configure count)
  if(NOT count EQUAL 1)
    message(FATAL_ERROR "${var}=${value} holds no source tree with configure. "
      "Set ${var} to download, a tarball, a source directory, or OFF.")
  endif()
  cmake_path(GET configure PARENT_PATH tree)
  set(${out} "${tree}" PARENT_SCOPE)
endfunction()

# salt_mpi_check_source(<var>)
#
# Stops with an error unless cache variable <var> is download, OFF, the
# path of a file, or a directory that has `configure`: the forms that
# salt_mpi_source() takes. Unlike salt_mpi_source(), this needs no
# network and unpacks nothing, so a build that makes no MPI files can
# still check the path.
function(salt_mpi_check_source var)
  set(value "${${var}}")
  set(stop "Set ${var} to download, a tarball, a source directory, or OFF.")
  if(NOT value OR value STREQUAL "download")
    return()
  elseif(IS_DIRECTORY "${value}")
    if(NOT EXISTS "${value}/configure")
      message(FATAL_ERROR "${var}=${value} is a directory without configure. ${stop}")
    endif()
  elseif(NOT EXISTS "${value}")
    message(FATAL_ERROR "${var}=${value} does not exist. ${stop}")
  endif()
endfunction()

# _salt_mpi_download(<var> <url> <sha256> <tarball> <ok>)
#
# Downloads <url> to <tarball>, unless <tarball> already has SHA-256
# <sha256>, for salt_mpi_source(). Sets <ok> to whether <tarball> then
# has it; when not, warns and removes <tarball>.
function(_salt_mpi_download var url sha256 tarball ok)
  set(${ok} TRUE PARENT_SCOPE)
  if(EXISTS "${tarball}")
    file(SHA256 "${tarball}" have)
    if(have STREQUAL sha256)
      return()
    endif()
  endif()
  message(STATUS "Downloading ${url}")
  # A stalled network warns, as a failed download does
  file(DOWNLOAD "${url}" "${tarball}" STATUS status INACTIVITY_TIMEOUT 60)
  list(GET status 0 code)
  list(GET status 1 reason)
  if(code EQUAL 0)
    file(SHA256 "${tarball}" have)
    if(have STREQUAL sha256)
      return()
    endif()
    set(reason "its SHA-256 is ${have}, not ${sha256}")
  endif()
  set(${ok} FALSE PARENT_SCOPE)
  file(REMOVE "${tarball}")
  message(WARNING
    "Could not download ${url}: ${reason}. SALT-FM is built without these "
    "MPI files. To build with them, download the file and set ${var} to "
    "its path. To build without them and without this warning, set "
    "${var}=OFF.")
endfunction()
