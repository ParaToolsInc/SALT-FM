# Runs salt_mpi_source() of cmake/modules/SALTMPI.cmake for each form of
# SALT_MPICH, in a fresh directory DIR. The source tree is FAKE, which
# has a configure file; the script packs it into a tarball, whose SHA-256
# the function expects unless FORM says otherwise. The script prints
# each warning and error of the function on one line, as `warning: ` or
# `error: ` and the text, and then `source tree: <dir>` for the tree
# that the function gives, which must have configure, or
# `source tree: none`. FORM is one of:
#   download        download, from a file:// URL of the tarball
#   download_again  the same, then again from a URL of no file, with
#                   the tarball already in DIR
#   download_hash   download, with another SHA-256
#   download_fail   download, from a file:// URL of no file
#   tarball         the path of the tarball
#   tarball_hash    the same, with another SHA-256
#   directory       FAKE
#   no_configure    a directory without configure
#   off             OFF
#   missing         a path that does not exist
#
# Usage: cmake -DFORM=<form> -DFAKE=<dir> -DDIR=<dir>
#          -P check_salt_mpi_source.cmake

foreach(arg FORM FAKE DIR)
  if("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "check_salt_mpi_source.cmake needs -D${arg}=<value>")
  endif()
endforeach()
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/modules/SALTMPI.cmake)

# CMake wraps the text of a warning or an error over lines; print it on
# one line, so that a test can match it
function(message mode)
  if(mode STREQUAL "WARNING" OR mode STREQUAL "FATAL_ERROR")
    string(CONCAT text ${ARGN})
    string(REPLACE "FATAL_ERROR" "error" mode "${mode}")
    string(TOLOWER "${mode}" mode)
    _message("${mode}: ${text}")
    if(mode STREQUAL "error")
      _message(FATAL_ERROR "Stopped")
    endif()
  else()
    _message(${ARGV})
  endif()
endfunction()

file(REMOVE_RECURSE "${DIR}")
file(MAKE_DIRECTORY "${DIR}/pack" "${DIR}/empty")
cmake_path(GET FAKE FILENAME _name)
file(COPY "${FAKE}" DESTINATION "${DIR}/pack")
set(_tarball "${DIR}/pack/${_name}.tar.gz")
execute_process(COMMAND ${CMAKE_COMMAND} -E tar czf "${_tarball}" "${_name}"
  WORKING_DIRECTORY "${DIR}/pack" COMMAND_ERROR_IS_FATAL ANY)
file(SHA256 "${_tarball}" _sha256)
set(_url "file://${_tarball}")
# The same file name, as salt_mpi_source() names the download after it
set(_no_url "file://${DIR}/none/${_name}.tar.gz")

if(FORM MATCHES "^download")
  set(SALT_MPICH download)
  if(FORM STREQUAL "download_fail")
    set(_url "${_no_url}")
  endif()
elseif(FORM MATCHES "^tarball")
  set(SALT_MPICH "${_tarball}")
elseif(FORM STREQUAL "directory")
  set(SALT_MPICH "${FAKE}")
elseif(FORM STREQUAL "no_configure")
  set(SALT_MPICH "${DIR}/empty")
elseif(FORM STREQUAL "off")
  set(SALT_MPICH OFF)
elseif(FORM STREQUAL "missing")
  set(SALT_MPICH "${DIR}/missing.tar.gz")
else()
  message(FATAL_ERROR "Unknown FORM ${FORM}")
endif()
if(FORM MATCHES "_hash$")
  string(REPEAT 0 64 _sha256)
endif()
if(FORM STREQUAL "download_again")
  salt_mpi_source(SALT_MPICH "${_url}" ${_sha256} "${DIR}/work" _tree)
  set(_url "${_no_url}")
endif()

salt_mpi_source(SALT_MPICH "${_url}" ${_sha256} "${DIR}/work" _tree)
if(_tree STREQUAL "")
  message("source tree: none")
elseif(EXISTS "${_tree}/configure")
  message("source tree: ${_tree}")
else()
  message(FATAL_ERROR "source tree ${_tree} has no configure")
endif()
