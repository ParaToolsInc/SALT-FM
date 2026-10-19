#--------------------------------
# Generic CMake settings for SALT
#--------------------------------

cmake_minimum_required(VERSION 3.23.0)
# Ensure policies are set as they have been tested
cmake_policy(VERSION 3.23.0...3.31.2)
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0042)
  # Use MACOSX_RPATH
  cmake_policy(SET CMP0042 NEW)
  set(CMAKE_MACOSX_RPATH ON)
endif()

# Big Sur is the oldest macOS supported by libc++ in LLVM 11+ (the floor
# we ship against). Targeting 10.15 emits "selected platform is no longer
# supported" warnings from <__configuration/availability.h>. The target
# may be raised again post-find_package(LLVM) to match the LLVM dylib's
# own LC_BUILD_VERSION minos. A cache entry, not a normal variable, so
# that raise takes effect in the first configure too: a normal variable
# would hide the raised cache entry (policy CMP0126).
if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
    "Minimum macOS deployment target")
endif()

# Auto-detect CMAKE_OSX_SYSROOT via xcrun if not set. Without it, CMake
# does not pass `-isysroot` and the active SDK's C headers come in via
# explicit `-isystem` entries (re-exported from LLVM's CMake config
# targets), which sit *higher* than libc++'s implicit search and break
# wrapper resolution for <cfloat> / <cmath> / <cinttypes> deep in our
# ryml_all.hpp / LLVM header chain. Must be set before `project()` so
# CMake's compiler probes and platform initialisation see it. xcrun
# ships with Xcode / Command Line Tools so it is always available on
# a configured macOS host.
if(APPLE AND NOT CMAKE_OSX_SYSROOT)
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE _salt_xcrun_sdk
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _salt_xcrun_sdk_rc)
  if(_salt_xcrun_sdk_rc EQUAL 0 AND _salt_xcrun_sdk
      AND IS_DIRECTORY "${_salt_xcrun_sdk}")
    message(STATUS "Auto-detected CMAKE_OSX_SYSROOT: ${_salt_xcrun_sdk}")
    set(CMAKE_OSX_SYSROOT "${_salt_xcrun_sdk}" CACHE PATH
      "macOS SDK path (auto-detected via xcrun)")
  endif()
  unset(_salt_xcrun_sdk)
  unset(_salt_xcrun_sdk_rc)
endif()

# Test if being built under a multi-config generator, i.e. and IDE like MSVS
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
# Organize MSVS projects with folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Look for our custom CMake Modules here: cmake/modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

# Specify _INIT variables to override platform/compiler defaults
# See https://cmake.org/cmake/help/v3.11/variable/CMAKE_USER_MAKE_RULES_OVERRIDE.html
# Platform/compiler/project specific flags are set in the USER_MAKE_RULES_OVERRIDE file
set(SALT_ALLOWED_BUILD_TYPES "Debug" "Release" "MinSizeRel" "RelWithDebInfo" "RuntimeDebug")

if(NOT isMultiConfig)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
  elseif(NOT CMAKE_BUILD_TYPE IN_LIST SALT_ALLOWED_BUILD_TYPES)
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${SALT_ALLOWED_BUILD_TYPES}")
endif()

# Only treat warnings as errors in Debug builds. Release builds tolerate new
# warnings introduced by newer compiler versions. Use a generator expression
# so multi-config generators (Visual Studio, Xcode, Ninja Multi-Config) honor
# the policy per-configuration -- a configure-time CMAKE_BUILD_TYPE check
# would be empty under those generators and silently drop -Werror.
set(SALT_WERROR "$<$<CONFIG:Debug>:-Werror>")

#------------------------------------------------------------
# SALT project specific global options, version, and defaults
#------------------------------------------------------------

# Ensure we're not building in the source tree root
include(CheckOutOfSourceBuild)
check_out_of_source_build()

# SALT project version
# Sources of truth (see cmake/modules/SALTVersion.cmake):
#   - .VERSION file (committed) -- canonical bare SemVer
#   - git describe (when .git/ present) -- adds build metadata for dev builds
include(SALTVersion)

# When configuring inside a git checkout, point the clone at the vendored
# .githooks/ directory so the version-drift pre-push hook is active.
include(SALTGitHooks)

# Print project logo, version and tag line
file(READ ${CMAKE_CURRENT_LIST_DIR}/cmake/SALT-logo.txt SALT_LOGO)
string(JOIN "\n         " SALT_GREETING "${SALT_LOGO}"
  "SALT-FM, v${SALT_VERSION_FULL}: An LLVM-based Source Analysis Toolkit for HPC")
if(NOT isMultiConfig)
  string(APPEND SALT_GREETING "\n         Build type: ${CMAKE_BUILD_TYPE}")
endif()
string(APPEND SALT_GREETING "\n")
string(PREPEND SALT_GREETING "\n")
message(STATUS "${SALT_GREETING}")

#---------------------------------------------------------------------------------
# Get & print diagnostics about flags passed to CMake and build/source directories
#---------------------------------------------------------------------------------
message(STATUS "CMake source directory: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMake binary directory: ${CMAKE_BINARY_DIR}")
include(GetPassedCMakeArgs)
get_passed_cmake_args()
if(CMAKE_ARGS)
  message("Passed command line CMAKE_ARGS: ${CMAKE_ARGS}")
endif()

set(CMAKE_CXX_STANDARD 17)

#-----------------------------
# Create the main SALT project
#-----------------------------
project(saltfm
  VERSION "${SALT_VERSION_BASE}"
  DESCRIPTION "An LLVM-based Source Analysis Toolkit for HPC"
  HOMEPAGE_URL "https://github.com/ParaToolsInc/SALT-FM"
  LANGUAGES CXX C)

# Force libc++ for the Homebrew/Spack/source-built Clang we require on
# macOS. Without the explicit flag the libc++ header search order
# falls back to the active SDK's C headers before libc++'s wrappers,
# producing "<cfloat> tried including <float.h> but didn't find
# libc++'s <float.h>" errors deep in our ryml_all.hpp / LLVM header
# chain. (SALT requires Clang, not AppleClang, so the condition is
# narrow.) Skipped if the user already selected a stdlib via
# CMAKE_CXX_FLAGS or CXXFLAGS.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    AND NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=")
  string(APPEND CMAKE_CXX_FLAGS " -stdlib=libc++")
endif()

include(GNUInstallDirs)

#----------
# Find LLVM
#----------

# See https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project for more
# information. If LLVM and/or Clang installations are partial, then their
# supplied CMake exports files need to be patched. This is already done in
# the salt-dev docker container. The patches are kept as a git submodule at
# ./cmake/llvm-patches and the upstream at
# https://github.com/ParaToolsInc/salt-llvm-patches. If either of the steps
# below fails and you have write access to the LLVM/Clang installations,
# try patching them using the supplied patches.

# Seed CMAKE_PREFIX_PATH from the active C++ compiler, llvm-config,
# and flang-new so find_package below locates non-default LLVM/Flang
# installs (Homebrew, Spack, conda, hand-built) without the user
# passing -DCMAKE_PREFIX_PATH. Toolchain-driven detection - no
# package-manager queries - so it works for any install whose bin/ is
# on PATH. Skipped entirely if CMAKE_PREFIX_PATH was already set by
# the user (cache or env).
function(_salt_seed_prefix tag path)
  if(NOT path OR "${path}" IN_LIST CMAKE_PREFIX_PATH)
    return()
  endif()
  message(STATUS "Seeding CMAKE_PREFIX_PATH from ${tag}: ${path}")
  list(APPEND CMAKE_PREFIX_PATH "${path}")
  set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

if(NOT CMAKE_PREFIX_PATH AND "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
  # `${CMAKE_CXX_COMPILER}` is the most reliable LLVM-prefix anchor:
  # if CMake's compiler probe just picked clang++ from a non-default
  # install, its <prefix>/bin/clang++ points straight at the LLVM
  # install root. Covers the common case where llvm-config was not
  # symlinked onto PATH (e.g. Homebrew keeps llvm-config in
  # /usr/local/opt/llvm/bin but only symlinks clang/clang++).
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
      OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    get_filename_component(_salt_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(_salt_cxx_prefix "${_salt_cxx_bin}" DIRECTORY)
    _salt_seed_prefix("${CMAKE_CXX_COMPILER}" "${_salt_cxx_prefix}")
    unset(_salt_cxx_bin)
    unset(_salt_cxx_prefix)
  endif()
  find_program(SALT_LLVM_CONFIG_EXE NAMES llvm-config
    DOC "llvm-config used to seed CMAKE_PREFIX_PATH")
  if(SALT_LLVM_CONFIG_EXE)
    execute_process(
      COMMAND ${SALT_LLVM_CONFIG_EXE} --prefix
      OUTPUT_VARIABLE _salt_llvm_prefix
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE _salt_llvm_prefix_rc)
    if(_salt_llvm_prefix_rc EQUAL 0)
      _salt_seed_prefix("${SALT_LLVM_CONFIG_EXE}" "${_salt_llvm_prefix}")
    endif()
    unset(_salt_llvm_prefix)
    unset(_salt_llvm_prefix_rc)
  endif()
  # Homebrew ships flang in a separate keg from llvm; Spack also stages
  # them under distinct prefixes. Also probe for flang-new and add its
  # `<prefix>` so find_package(Flang CONFIG) can resolve.
  find_program(SALT_FLANG_NEW_EXE NAMES flang-new flang
    DOC "flang driver used to seed CMAKE_PREFIX_PATH")
  if(SALT_FLANG_NEW_EXE)
    get_filename_component(_salt_flang_bin "${SALT_FLANG_NEW_EXE}" DIRECTORY)
    get_filename_component(_salt_flang_prefix "${_salt_flang_bin}" DIRECTORY)
    _salt_seed_prefix("${SALT_FLANG_NEW_EXE}" "${_salt_flang_prefix}")
    unset(_salt_flang_bin)
    unset(_salt_flang_prefix)
  endif()
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# On macOS, the LLVM dylib we link against often declares a higher
# LC_BUILD_VERSION minos than CMAKE_OSX_DEPLOYMENT_TARGET (Homebrew and
# Spack bottles target the build host's macOS). Linking that combination
# produces a swarm of "object file ... built for newer macOS version"
# warnings from ld and a portability promise we cannot honour anyway.
# Resolve the dylib via the compiler itself (-print-file-name) so this
# works for Homebrew, Spack, conda, or a hand-built LLVM. If the probe
# fails or the value is lower, the 11.0 floor stays in place.
if(APPLE)
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libLLVM.dylib
    OUTPUT_VARIABLE _salt_llvm_dylib
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _salt_pf_rc)
  if(_salt_pf_rc EQUAL 0
      AND _salt_llvm_dylib
      AND NOT _salt_llvm_dylib STREQUAL "libLLVM.dylib"
      AND EXISTS "${_salt_llvm_dylib}")
    execute_process(
      COMMAND otool -l "${_salt_llvm_dylib}"
      OUTPUT_VARIABLE _salt_otool_out
      RESULT_VARIABLE _salt_otool_rc)
    if(_salt_otool_rc EQUAL 0)
      string(REGEX MATCH "minos[ \t]+([0-9]+\\.[0-9]+)"
             _ "${_salt_otool_out}")
      if(CMAKE_MATCH_1
          AND CMAKE_MATCH_1 VERSION_GREATER
              "${CMAKE_OSX_DEPLOYMENT_TARGET}")
        message(STATUS
          "Raising CMAKE_OSX_DEPLOYMENT_TARGET "
          "${CMAKE_OSX_DEPLOYMENT_TARGET} -> ${CMAKE_MATCH_1} "
          "to match ${_salt_llvm_dylib}")
        # FORCE so cmake-gui / ccmake and subsequent re-configures see
        # the raised value; CMAKE_OSX_DEPLOYMENT_TARGET is a cache
        # variable, so a plain set() would only update the local scope
        # and let the stale cache entry win on the next configure.
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${CMAKE_MATCH_1}" CACHE STRING
          "Minimum macOS deployment target (raised to match LLVM dylib)"
          FORCE)
      endif()
    endif()
  endif()
endif()

# get_cmake_property(_variableNames VARIABLES)
# list(SORT _variableNames)
# foreach(_variableName ${_variableNames})
#   message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

# Available LLVM components are stored in $LLVM_AVAILABLE_LIBS and can be found with
# `llvm-config --components`
# Set the LLVM components needed by salt, see tools in the llvm-project/clang/tools/ and
# llvm-project/clang-tools-extra for examples of CMakeLists.txt
set(LLVM_NEEDED_COMPONENTS
  option
  passes
)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_NEEDED_COMPONENTS})

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in:\n\t${LLVM_DIR}")
message(STATUS "LLVM include directory(s):\n\t${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM_LIBS (used by salt): ${LLVM_LIBS}")
message(STATUS "LLVM_DEFINITIONS: ${LLVM_DEFINITIONS}")
message(STATUS "LLVM_ENABLE_ASSERTIONS: ${LLVM_ENABLE_ASSERTIONS}")
message(STATUS "LLVM_ENABLE_EH: ${LLVM_ENABLE_EH}")
message(STATUS "LLVM_ENABLE_RTTI: ${LLVM_ENABLE_RTTI}")
if(NOT LLVM_ENABLE_RTTI)
  set(USE_RTTI -fno-rtti)
  message(STATUS
    "LLVM built with ENABLE_RTTI=FALSE, -fno-rtti will be passed when building & linking")
endif()
message(STATUS "LLVM_TOOLS_BINARY_DIR:\n\t${LLVM_TOOLS_BINARY_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

list(LENGTH CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES NUM_CLANG_HEADER_INCLUDES)
list(JOIN CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "\", \"-I" CLANG_HEADER_INCLUDES)
set(CLANG_HEADER_INCLUDES "\"-I${CLANG_HEADER_INCLUDES}\"")

# On macOS, libtooling needs -isysroot and the host clang's -resource-dir
# to satisfy libc++'s wrapped C headers (e.g. <cstddef> -> <stddef.h>).
# Without these, parsing user TUs fails with "tried including <stddef.h>
# but didn't find libc++'s <stddef.h>".
if(APPLE)
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE SALT_CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _print_resource_dir_rc)
  if(_print_resource_dir_rc EQUAL 0
      AND SALT_CLANG_RESOURCE_DIR
      AND CMAKE_OSX_SYSROOT)
    string(APPEND CLANG_HEADER_INCLUDES
      ", \"-isysroot\", \"${CMAKE_OSX_SYSROOT}\""
      ", \"-resource-dir\", \"${SALT_CLANG_RESOURCE_DIR}\"")
    math(EXPR NUM_CLANG_HEADER_INCLUDES "${NUM_CLANG_HEADER_INCLUDES} + 4")
  endif()
endif()

message(STATUS "CLANG_HEADER_INCLUDES: ${CLANG_HEADER_INCLUDES}")
message(STATUS "NUM_CLANG_HEADER_INCLUDES: ${NUM_CLANG_HEADER_INCLUDES}")

# Fail-fast probe: the deepest external header in our include chain is
# the vendored ryml_all.hpp. On macOS a misconfigured C++ toolchain
# (e.g. Homebrew clang++ against the Command Line Tools SDK rather
# than the Xcode SDK, or libc++ wrapped C headers without a matching
# resource-dir) reliably blows up parsing this header from the actual
# build. Probe at configure time so misconfig surfaces here with a
# README pointer instead of as opaque header errors mid-build.
include(CheckCXXSourceCompiles)
set(_salt_ryml_save_includes "${CMAKE_REQUIRED_INCLUDES}")
set(_salt_ryml_save_flags "${CMAKE_REQUIRED_FLAGS}")
list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_SOURCE_DIR}/include")
# CMAKE_REQUIRED_FLAGS is documented as a space-separated string, not a
# CMake list. `list(APPEND ...)` would introduce a `;` separator that
# Check*SourceCompiles' downstream `CMAKE_FLAGS -DCOMPILE_DEFINITIONS:
# STRING=${CMAKE_REQUIRED_FLAGS}` splice would silently truncate.
string(APPEND CMAKE_REQUIRED_FLAGS " -std=c++17")
check_cxx_source_compiles("
#define RYML_SINGLE_HDR_DEFINE_NOW
#define RYML_SHARED
#include <ryml_all.hpp>
int main(void) { return 0; }
" SALT_RYML_HEADER_OK)
set(CMAKE_REQUIRED_INCLUDES "${_salt_ryml_save_includes}")
set(CMAKE_REQUIRED_FLAGS "${_salt_ryml_save_flags}")
unset(_salt_ryml_save_includes)
unset(_salt_ryml_save_flags)
if(NOT SALT_RYML_HEADER_OK)
  set(_salt_ryml_hint "")
  if(APPLE)
    set(_salt_ryml_hint
      " On macOS this typically means the chosen clang++ cannot resolve "
      "libc++ wrapped C headers against the active SDK. See the macOS "
      "section of README.md and verify the compiler selection first. "
      "If auto-detection does not work in your environment, you may need "
      "to override CMAKE_PREFIX_PATH, CMAKE_OSX_SYSROOT, or pass "
      "-stdlib=libc++ manually.")
  endif()
  message(FATAL_ERROR
    "Cannot compile <ryml_all.hpp> with the configured C++ toolchain."
    "${_salt_ryml_hint}")
endif()

#------------------------------------------
# Specify clang and system libraries needed
#------------------------------------------
# There should be a better way of doing this, but the llvm libtooling example just puts
# user written clang tools within the llvm/clang source tree....
set(CLANG_LIBS
  clangFrontend
  clangSerialization
  clangDriver
  clangTooling
  clangParse
  clangSema
  clangAnalysis
  clangEdit
  clangAST
  clangLex
  clangBasic
  clangRewrite
  clangRewriteFrontend
)

#-----------------------------------------
# Add an interface library to link against
#-----------------------------------------

# This provides an abstraction and shorthand so that you can just `target_link_libraries()` against
# the SALT_LLVM_TOOLING interface library
add_library(SALT_LLVM_TOOLING INTERFACE)
target_compile_features(SALT_LLVM_TOOLING INTERFACE cxx_std_17)
target_include_directories(SALT_LLVM_TOOLING INTERFACE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(SALT_LLVM_TOOLING INTERFACE ${LLVM_DEFINITIONS_LIST})
target_compile_options(SALT_LLVM_TOOLING INTERFACE
  ${USE_RTTI} -Wall -Wpedantic -Wno-unused-variable
  $<$<CONFIG:Debug>:-Wno-gnu-zero-variadic-macro-arguments>)
target_link_libraries(SALT_LLVM_TOOLING INTERFACE ${CLANG_LIBS} ${LLVM_LIBS})

#---------------------------------
# List the header and source files
#---------------------------------

# Explicitly listing source files is safer than globbing them--you might accidentally put
# an experiment or back up file in include or src and not realize it.
set(SALT_HEADER_FILES
  dprint.hpp
  ryml_all.hpp
  selectfile.hpp
  instrumentor.hpp
  sema_fabricator.hpp
)

list(TRANSFORM SALT_HEADER_FILES PREPEND "${CMAKE_SOURCE_DIR}/include/")
foreach(header clang_header_includes.h frontend.hpp)
  configure_file(
    "${CMAKE_SOURCE_DIR}/include/${header}.in"
    "${CMAKE_BINARY_DIR}/include/${header}"
    @ONLY)
  list(APPEND SALT_HEADER_FILES "${CMAKE_BINARY_DIR}/include/${header}")
  set_source_files_properties(
    "${CMAKE_BINARY_DIR}/include/${header}"
    PROPERTIES
    GENERATED TRUE
  )
endforeach()

set(CPARSE_LLVM_SRCS
  frontend.cpp
  instrumentor.cpp
  selectfile.cpp
  sema_fabricator.cpp
)

list(TRANSFORM CPARSE_LLVM_SRCS PREPEND "${CMAKE_SOURCE_DIR}/src/")

#---------------------
# Add the main targets
#---------------------
# If we refactor into a library that executables link against it will simplify
# this and reduce repetition
add_executable(cparse-llvm)
target_sources(cparse-llvm PUBLIC ${CPARSE_LLVM_SRCS})
target_sources(cparse-llvm
  PUBLIC
  FILE_SET headers
  TYPE HEADERS
  FILES ${SALT_HEADER_FILES}
  BASE_DIRS ${CMAKE_SOURCE_DIR}/include;${CMAKE_BINARY_DIR}/include)
target_include_directories(cparse-llvm PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/include")
target_compile_features(cparse-llvm PUBLIC cxx_std_17)
# Inherit definitions, compile features, etc.
target_link_libraries(cparse-llvm PUBLIC SALT_LLVM_TOOLING)
# You can try adding -static. The --as-needed/--no-allow-shlib-undefined/
# --no-undefined flags are GNU ld extensions; Apple ld doesn't recognize
# them, so gate on linker family.
target_link_options(cparse-llvm PUBLIC
  $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--as-needed>
  $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--no-allow-shlib-undefined>
  $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--no-undefined>)
# Turn on debug output if a debug build is being built
target_compile_definitions(cparse-llvm PUBLIC $<$<CONFIG:Debug>:DEBUG_NO_WAY>)
# Install the target
install(TARGETS cparse-llvm DESTINATION ${CMAKE_INSTALL_BINDIR})
set_target_properties(cparse-llvm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")

#------------------------------------------------------
# Handle config files in build directory & installation
#------------------------------------------------------
# Copy ${CMAKE_SOURCE_DIR}/config_files to
# ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/config_files
# and install them to ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/config_files
file(COPY ${CMAKE_SOURCE_DIR}/config_files
  DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME})
install(DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
  DESTINATION ${CMAKE_INSTALL_DATADIR})

#---------------------
# Flang Frontend library
#---------------------

# Flang package requires MLIR
find_package(MLIR CONFIG)
if(MLIR_FOUND)
  message(STATUS "Found MLIR -- will check for Flang")
  find_package(Flang CONFIG)
else()
  message(STATUS "MLIR not found -- skipping Flang")
endif()

if(MLIR_FOUND AND Flang_FOUND)
  message(STATUS "Found Flang -- will build Flang frontend plugin")
  set(SALT_MOD_SUFFIX ".saltmod" CACHE STRING
    "Suffix for SALT-FM Fortran intrinsic .mod files")
  set(SALT_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR}/salt/flang
    CACHE STRING "SALT include directory for Flang intrinsic modules")
  set(TEST_FORTRAN TRUE)

  # Find the llvm-flang directory containing the intrinsic modules so that we
  # can copy them and give them the SALT_MOD_SUFFIX
  list(LENGTH FLANG_INCLUDE_DIRS NUM_FLANG_INCLUDE_DIRS)
  message(DEBUG "NUM_FLANG_INCLUDE_DIRS: ${NUM_FLANG_INCLUDE_DIRS}")
  if(NUM_FLANG_INCLUDE_DIRS EQUAL 1)
    set(FLANG_INTRINSIC_MOD_DIR ${FLANG_INCLUDE_DIRS})
  else()
    set(FLANG_INTRINSIC_MOD_DIR ${FLANG_INSTALL_PREFIX})
  endif()
  message(DEBUG "FLANG_INTRINSIC_MOD_DIR: ${FLANG_INTRINSIC_MOD_DIR}")
  # Flang 23+ installs them in <resource-dir>/finclude/flang/<triple>,
  # the path its driver passes to the frontend; ask the driver for both,
  # preferring the one installed with the Flang package found above.
  find_program(_salt_flang_driver NAMES flang-new flang NAMES_PER_DIR
    PATHS "${FLANG_INSTALL_PREFIX}/bin" "${LLVM_TOOLS_BINARY_DIR}"
    NO_DEFAULT_PATH NO_CACHE)
  if(NOT _salt_flang_driver)
    find_program(_salt_flang_driver NAMES flang-new flang NAMES_PER_DIR
      NO_CACHE)
  endif()
  if(_salt_flang_driver)
    execute_process(COMMAND ${_salt_flang_driver} -print-resource-dir
      OUTPUT_VARIABLE _salt_flang_resource_dir
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${_salt_flang_driver} -print-target-triple
      OUTPUT_VARIABLE _salt_flang_triple
      OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
  message(STATUS "Flang driver for intrinsic modules: ${_salt_flang_driver}")
  if(_salt_flang_resource_dir AND _salt_flang_triple)
    set(_salt_flang_mod_hint
      "${_salt_flang_resource_dir}/finclude/flang/${_salt_flang_triple}")
  endif()
  find_path(FLANG_INTRINSIC_MODS
    NAMES
      "ISO_Fortran_env.mod"
      "iso_fortran_env.mod"
      "ISO_fortran_env.mod"
      "iso_Fortran_env.mod"
    HINTS ${FLANG_INCLUDE_DIRS} ${_salt_flang_mod_hint}
    PATH_SUFFIXES "flang" "${CMAKE_INSTALL_INCLUDEDIR}/flang"
    REQUIRED
  )
  unset(_salt_flang_resource_dir)
  unset(_salt_flang_triple)
  unset(_salt_flang_mod_hint)
  message(STATUS "FLANG_INTRINSIC_MODS_DIR: ${FLANG_INTRINSIC_MODS}")

  # Get a list of intrinsic module files that we will need to copy and change
  # the extension for
  file(GLOB INTRINSIC_MOD_FILES
    LIST_DIRECTORIES false
    ${FLANG_INTRINSIC_MODS}/*.mod
  )
  message(STATUS "Found flang intrinsic module files: ${INTRINSIC_MOD_FILES}")

  # Variables set in FlangConfig.cmake
  message(STATUS "FLANG_CMAKE_DIR: ${FLANG_CMAKE_DIR}")
  message(STATUS "FLANG_EXPORTED_TARGETS: ${FLANG_EXPORTED_TARGETS}")
  message(STATUS "FLANG_INCLUDE_DIRS: ${FLANG_INCLUDE_DIRS}")

  # Interface for Flang frontend plugins
  add_library(SALT_FLANG_FRONTEND INTERFACE)
  target_compile_features(SALT_FLANG_FRONTEND INTERFACE cxx_std_17)
  target_include_directories(SALT_FLANG_FRONTEND INTERFACE
    ${LLVM_INCLUDE_DIRS} ${FLANG_INCLUDE_DIRS})
  target_compile_definitions(SALT_FLANG_FRONTEND INTERFACE ${LLVM_DEFINITIONS_LIST})
  target_compile_options(SALT_FLANG_FRONTEND INTERFACE
    ${USE_RTTI} -Wall -Wno-unused-variable -Wpedantic
    $<$<CONFIG:Debug>:-Werror>)

  # Endianness definitions are required, and Flang does not export a definitions list
  include(TestBigEndian)
  test_big_endian(IS_BIGENDIAN)
  if(IS_BIGENDIAN)
    target_compile_definitions(SALT_FLANG_FRONTEND INTERFACE FLANG_BIG_ENDIAN=1)
  else()
    target_compile_definitions(SALT_FLANG_FRONTEND INTERFACE FLANG_LITTLE_ENDIAN=1)
  endif()

  set(SALT_FLANG_PLUGIN_HEADER_FILES
    dprint.hpp
    selectfile.hpp
    flang_source_location.hpp
    flang_instrumentation_constants.hpp
    flang_instrumentation_point.hpp
  )
  list(TRANSFORM SALT_FLANG_PLUGIN_HEADER_FILES PREPEND "${CMAKE_SOURCE_DIR}/include/")

  set(SALT_FLANG_PLUGIN_SRCS
    dprint.cpp
    selectfile.cpp
    flang_source_location.cpp
    flang_instrumentation_point.cpp
    flang_salt_instrument_plugin.cpp
  )
  list(TRANSFORM SALT_FLANG_PLUGIN_SRCS PREPEND "${CMAKE_SOURCE_DIR}/src/")

  add_library(salt-flang-plugin SHARED)
  target_sources(salt-flang-plugin PUBLIC ${SALT_FLANG_PLUGIN_SRCS})
  target_sources(salt-flang-plugin
    PUBLIC
    FILE_SET headers
    TYPE HEADERS
    FILES ${SALT_FLANG_PLUGIN_HEADER_FILES})
  target_include_directories(salt-flang-plugin PUBLIC
    "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/include")
  target_compile_features(salt-flang-plugin PUBLIC cxx_std_17)
  target_link_libraries(salt-flang-plugin PUBLIC SALT_FLANG_FRONTEND)
  # Apple ld uses `-undefined dynamic_lookup`; GNU ld uses --as-needed and
  # rejects -undefined dynamic_lookup. Pick per linker.
  target_link_options(salt-flang-plugin PUBLIC
    $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--as-needed>
    $<$<PLATFORM_ID:Darwin>:-Wl,-undefined>
    $<$<PLATFORM_ID:Darwin>:-Wl,dynamic_lookup>)
  set_target_properties(salt-flang-plugin PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}")

  install(TARGETS salt-flang-plugin DESTINATION ${CMAKE_INSTALL_LIBDIR})

  # Configure fparse-llvm at `out`, recording Flang driver
  # `salt_flang_driver` (nothing if configure found none); it runs that
  # driver by default.
  function(_salt_configure_fparse_llvm out salt_flang_driver)
    if(NOT salt_flang_driver)
      set(salt_flang_driver "")
    endif()
    configure_file(${CMAKE_SOURCE_DIR}/src/fparse-llvm.in ${out} @ONLY)
  endfunction()
  _salt_configure_fparse_llvm(
    ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm
    "${_salt_flang_driver}")
  unset(_salt_flang_driver)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm
    TYPE BIN) # TYPE BIN installs into CMAKE_INSTALL_BINDIR

  # Copy the flang intrinsic module files into the salt include directory in
  # the build tree and change the file extension to SALT_MOD_SUFFIX.
  # configure_file copies the contents: Homebrew's modules are relative
  # symlinks, which would dangle in the build tree.
  foreach(intrinsic_mod_file IN LISTS INTRINSIC_MOD_FILES)
    get_filename_component(INTRINSIC_MOD_BASE ${intrinsic_mod_file} NAME_WLE)
    set(SALT_INTRINSIC_MOD_FILE
      ${CMAKE_BINARY_DIR}/${SALT_INC_DIR}/${INTRINSIC_MOD_BASE}${SALT_MOD_SUFFIX})
    message(STATUS "Copying ${intrinsic_mod_file} to ${SALT_INTRINSIC_MOD_FILE}")
    configure_file(${intrinsic_mod_file} ${SALT_INTRINSIC_MOD_FILE} COPYONLY)
  endforeach()

  # Install the flang intrinsic module files into the salt include directory
  install(DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}/salt
    TYPE INCLUDE)

else()
  message(STATUS "Flang not found -- skipping Flang frontend plugin")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/src/saltfm.in
  ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
  @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
  TYPE BIN) # TYPE BIN installs into CMAKE_INSTALL_BINDIR

#---------------------
# Find TAU locations for testing
#---------------------

# Resolve TAU_ROOT in priority order: explicit -DTAU_ROOT cache var,
# then $TAU_ROOT environment, then auto-detected from `tau_exec` on PATH.
# All three paths land in the same cache var so subsequent configures
# and cache inspectors see one consistent value.
set(TAU_PATH_SUFFIXES x86_64 x86_64/bin craycnl craycnl/bin apple apple/bin)
if(NOT TAU_ROOT AND DEFINED ENV{TAU_ROOT})
  set(TAU_ROOT "$ENV{TAU_ROOT}" CACHE PATH "TAU Root Directory")
endif()
if(NOT TAU_ROOT)
  find_program(TAU_EXEC tau_exec
    PATH_SUFFIXES ${TAU_PATH_SUFFIXES}
  )
  if(NOT TAU_EXEC)
    message(STATUS "TAU not found. Please set TAU_ROOT to the TAU installation directory.")
  else()
    get_filename_component(_tau_root ${TAU_EXEC} DIRECTORY) # bin directory
    get_filename_component(_tau_root ${_tau_root} DIRECTORY) # arch directory
    string(REGEX REPLACE "(/x86_64$)|(/apple$)|(/craycnl$)" ""
      _tau_root "${_tau_root}")
    set(TAU_ROOT "${_tau_root}" CACHE PATH "TAU Root Directory")
    unset(_tau_root)
  endif()
endif()

# Find the TAU makefiles, executables, scripts, libraries, etc.
message(STATUS "TAU_ROOT: ${TAU_ROOT}")

find_program(TAU_EXEC tau_exec
  PATHS ${TAU_ROOT}/x86_64 ${TAU_ROOT}/apple ${TAU_ROOT}/craycnl
  PATH_SUFFIXES bin
)
# Hardening switch: when ON, configure fails if TAU is present but any
# required artifact is missing. Default OFF so end-user installs degrade
# gracefully (HAVE_TAU=FALSE skips TAU-dependent tests). CI sets this ON.
option(SALT_REQUIRE_TAU
  "Fail configure if TAU is present but any required artifact is missing"
  OFF)

if(TAU_EXEC)
  get_filename_component(TAU_ARCH_DIR ${TAU_EXEC} DIRECTORY)
  get_filename_component(TAU_ARCH_DIR ${TAU_ARCH_DIR} DIRECTORY)
  message(STATUS "TAU_ARCH_DIR: ${TAU_ARCH_DIR}")
  # tau_exec / tau_cc.sh / tau_f90.sh internally call sibling helpers
  # (tau-config, tau_compiler.sh) by bare name, requiring TAU's bin
  # directory on PATH. Distros that install TAU into /usr/local pick this
  # up implicitly; in-tree installs (common on macOS) do not. Tests below
  # prepend ${TAU_BIN_DIR} via ENVIRONMENT_MODIFICATION.
  set(TAU_BIN_DIR "${TAU_ARCH_DIR}/bin")

  find_file(TAU_LLVM_MAKEFILE
    NAMES Makefile.tau-clang-pthread Makefile.tau-clang-pthread-pdt
    PATHS ${TAU_ARCH_DIR} PATH_SUFFIXES lib
  )
  find_file(TAU_GCC_MAKEFILE
    NAMES Makefile.tau-pthread Makefile.tau-pthread-pdt
    PATHS ${TAU_ARCH_DIR} PATH_SUFFIXES lib
  )
  find_program(TAUCC tau_cc.sh
    PATHS ${TAU_ARCH_DIR} PATH_SUFFIXES bin
  )
  find_program(TAUCXX tau_cxx.sh
    PATHS ${TAU_ARCH_DIR} PATH_SUFFIXES bin
  )
  find_program(TAUF90 tau_f90.sh
    PATHS ${TAU_ARCH_DIR} PATH_SUFFIXES bin
  )

  set(_salt_tau_missing "")
  foreach(tau_var TAU_LLVM_MAKEFILE TAU_GCC_MAKEFILE TAUCC TAUCXX TAUF90)
    if(NOT ${tau_var})
      list(APPEND _salt_tau_missing ${tau_var})
    endif()
  endforeach()
  if(_salt_tau_missing)
    set(HAVE_TAU FALSE)
    set(_msg
      "TAU found at ${TAU_ROOT} but missing required artifacts: "
      "${_salt_tau_missing}. TAU-dependent tests will be skipped.")
    if(SALT_REQUIRE_TAU)
      message(FATAL_ERROR "${_msg} (SALT_REQUIRE_TAU=ON)")
    else()
      message(WARNING "${_msg}")
    endif()
  else()
    set(HAVE_TAU TRUE)
  endif()
  unset(_salt_tau_missing)
else()
  set(HAVE_TAU FALSE)
  if(SALT_REQUIRE_TAU)
    message(FATAL_ERROR "SALT_REQUIRE_TAU=ON but tau_exec not found under ${TAU_ROOT}")
  endif()
endif()

# Derive the tau_exec `-T` token list from a Makefile.tau-<variant> path.
# `tau-config --binding` is filename-driven: with two installed variants
# (e.g. `Makefile.tau-pthread` and `Makefile.tau-clang-pthread`) plain
# `-T serial,pthread` matches both and the shortest-binding tiebreaker
# silently picks the gcc-stack variant. On macOS that mismatches the
# clang-stack TAU statically linked into the test binary. Encoding every
# `-` separated token from the variant name into the `-T` list narrows
# the match to a single binding directory regardless of toolchain.
function(_salt_tau_exec_tags out_var tau_makefile)
  get_filename_component(_mk "${tau_makefile}" NAME)
  string(REGEX REPLACE "^Makefile\\.tau-" "" _tags "${_mk}")
  string(REPLACE "-" "," _tags "${_tags}")
  set(${out_var} "serial,${_tags}" PARENT_SCOPE)
endfunction()

#---------------
# Tests
#---------------
# There are some assumptions made in testing:
# 1. Test source files to be instrumented are located in ${CMAKE_SOURCE_DIR}/tests/
# 2. Tests are currently not using MPI
# 3. Only pthread threading is assumed
# 4. TAU is installed with the following configuratins:
#    - Makefile.tau-clang-pthread
#    - Makefile.tau-pthread-pdt

# Care has been taken to break test inter-dependencies where possible and
# express them explicitly. A test that reads another test's output
# requires a fixture named after that test (FIXTURES_SETUP), so
# `ctest -R <check>` also runs the producer, and a failed producer
# leaves its checks Not Run.
# Also each test should not enter a race condition or need to use the
# RESOURCE_LOCK property.
# e.g., Executable names are different when compiled with gcc vs clang and
# each test using its own PROFILE_DIR so as not to clobber profile.n.n.n files.
# As a result you may run the tests in parallel via something like:
# `ctest --output-on-failure -j 20`

# List of tests to run from ${CMAKE_SOURCE_DIR}/tests/
# When adding more test sources, add them here.
set(TESTS_LIST
  hello.c
  1d.c
  c11test.cpp
  forward_decl.c
  pure_virtual.cpp
)

include(CTest)

# TAU's compiler wrappers split the saltfm and source paths at spaces,
# and some test checks match paths with `[^ ]*`, so tests would fail.
if(BUILD_TESTING)
  foreach(path_to_check IN ITEMS "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
    if(path_to_check MATCHES " ")
      message(FATAL_ERROR
        "The tests cannot run in '${path_to_check}': TAU's compiler wrappers "
        "and the test checks cannot handle spaces in paths. Use source and "
        "build directories whose paths have no spaces, or configure with "
        "-DBUILD_TESTING=OFF.")
    endif()
  endforeach()
endif()

# Tests write their outputs under test-output/<subdir>, never into the
# build root. `clean-test-artifacts` removes test-output/; the built-in
# `clean` also removes it and CTest's Testing/ directory.
set(SALT_TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-output)
add_custom_target(clean-test-artifacts
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${SALT_TEST_OUTPUT_DIR}
  COMMENT "Removing ${SALT_TEST_OUTPUT_DIR}"
  VERBATIM)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
  ${SALT_TEST_OUTPUT_DIR} ${CMAKE_BINARY_DIR}/Testing)

# Fixture <sub>_dir creates test-output/<sub>, so `ctest -R <test>` works
# in a fresh build directory. The TAU and user-mod fixtures are below.
set(_salt_test_output_subdirs instrument sif fabricate output_error)
if(TEST_FORTRAN)
  list(APPEND _salt_test_output_subdirs flang_exe)
endif()
foreach(sub IN LISTS _salt_test_output_subdirs)
  add_test(NAME setup_${sub}_dir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SALT_TEST_OUTPUT_DIR}/${sub})
  set_tests_properties(setup_${sub}_dir
    PROPERTIES
    FIXTURES_SETUP ${sub}_dir
    LABELS "phase:setup"
  )
endforeach()

# Run the tests in ARGN in test-output/<sub>, after its fixture created it.
function(_salt_in_test_output sub)
  set_tests_properties(${ARGN}
    PROPERTIES WORKING_DIRECTORY ${SALT_TEST_OUTPUT_DIR}/${sub})
  set_property(TEST ${ARGN} APPEND PROPERTY FIXTURES_REQUIRED ${sub}_dir)
endfunction()

# Test label helpers. Labels are grouped by key:value so "Label Time
# Summary" buckets meaningfully and `ctest -L "^lang:"` (etc.) filters
# by dimension.
#   lang:{C,CXX,Fortran}
#   toolchain:{GCC,LLVM}
#   phase:{smoke,instrument,compile,run,check,setup}
function(_salt_lang_to_label lang_in out_var)
  string(TOLOWER "${lang_in}" _l)
  if(_l STREQUAL "c")
    set(${out_var} "lang:C" PARENT_SCOPE)
  elseif(_l STREQUAL "cpp" OR _l STREQUAL "cxx" OR _l STREQUAL "c++")
    set(${out_var} "lang:CXX" PARENT_SCOPE)
  elseif(_l STREQUAL "fortran" OR _l MATCHES "^f(77|90|03|08)?$")
    set(${out_var} "lang:Fortran" PARENT_SCOPE)
  else()
    set(${out_var} "lang:unknown" PARENT_SCOPE)
  endif()
endfunction()

# Derive a `lang:<L>` ctest label from a source filename's extension by
# stripping the leading dot and delegating to _salt_lang_to_label.
function(_salt_ext_to_label src out_var)
  get_filename_component(_ext ${src} LAST_EXT)
  string(REGEX REPLACE "^\\." "" _ext "${_ext}")
  _salt_lang_to_label("${_ext}" _label)
  set(${out_var} ${_label} PARENT_SCOPE)
endfunction()

# Map a compiler driver name (gcc, clang, gfortran, flang-new, ...) to a
# `toolchain:<vendor>` ctest label. GCC family -> GCC; Clang/Flang -> LLVM.
function(_salt_compiler_to_toolchain comp out_var)
  string(TOLOWER "${comp}" _c)
  # Anchor to full-string match so e.g. `clang++` does not match the
  # `g\+\+` alternative; allow an optional `-<version>` suffix
  # (gcc-13, clang-17, 17.0.4, ...).
  if(_c MATCHES "^(gcc|g\\+\\+|gfortran)(-[0-9]+(\\.[0-9]+)*)?$")
    set(${out_var} "toolchain:GCC" PARENT_SCOPE)
  elseif(_c MATCHES "^(clang(\\+\\+)?|flang(-new)?)(-[0-9]+(\\.[0-9]+)*)?$")
    set(${out_var} "toolchain:LLVM" PARENT_SCOPE)
  else()
    set(${out_var} "toolchain:${comp}" PARENT_SCOPE)
  endif()
endfunction()

# Map a compiler driver name to its toolchain build subdir (GCC or LLVM).
# Same vendor classification as _salt_compiler_to_toolchain, but bare name
# (no `toolchain:` prefix) suitable for use as a test-output/tau subdir
# and as the inflection in TAU_<dir>_MAKEFILE variable lookups.
function(_salt_compiler_to_dir comp out_var)
  string(TOLOWER "${comp}" _c)
  # Same vendor classification as _salt_compiler_to_toolchain; see that
  # function for the regex rationale (full-string anchor + optional
  # `-<version>` suffix).
  if(_c MATCHES "^(gcc|g\\+\\+|gfortran)(-[0-9]+(\\.[0-9]+)*)?$")
    set(${out_var} "GCC" PARENT_SCOPE)
  elseif(_c MATCHES "^(clang(\\+\\+)?|flang(-new)?)(-[0-9]+(\\.[0-9]+)*)?$")
    set(${out_var} "LLVM" PARENT_SCOPE)
  else()
    string(TOUPPER "${comp}" _u)
    set(${out_var} "${_u}" PARENT_SCOPE)
  endif()
endfunction()

# Smoke test to check instrumentor compiled and will run
add_test(NAME salt_parser_smoke_test
  COMMAND $<TARGET_FILE:cparse-llvm> --help)
set_tests_properties(salt_parser_smoke_test
  PROPERTIES
  LABELS "phase:smoke"
  PASS_REGULAR_EXPRESSION "OVERVIEW"
)

# Map an entry from TESTS_LIST to a saltfm parser test (with a follow-up
# check that the .inst file was emitted). Optional second argument overrides
# the derived test name; passing it is somewhat fragile and likely more
# trouble than it's worth.
function(add_instrumentor_test test_src)
  get_filename_component(test_base_name ${test_src} NAME_WE)
  set(test_name "instrument_${test_base_name}")
  get_filename_component(test_lang ${test_src} LAST_EXT)
  set(extra_args ${ARGN})
  list(LENGTH extra_args n_extra_args)
  if(n_extra_args GREATER 1)
    message(AUTHOR_WARNING
      "Incorrect number of arguments (1 + ${n_extra_args}) passed to add_instrumentor_test!")
    return()
  endif()
  if(n_extra_args EQUAL 1)
    set(test_name ${ARGV1})
  endif()
  _salt_ext_to_label(${test_src} _lang_label)
  add_test(NAME ${test_name}
    COMMAND
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
      ${CMAKE_SOURCE_DIR}/tests/${test_src})
  set_tests_properties(${test_name}
    PROPERTIES
    REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/${test_src}"
    FIXTURES_SETUP ${test_name}
    LABELS "${_lang_label};phase:instrument"
    PASS_REGULAR_EXPRESSION "[Ii]nstrumentation:"
  )
  add_test(NAME ${test_name}_exists
    COMMAND ${CMAKE_COMMAND} -E cat ./${test_base_name}.inst${test_lang}
  )
  set_tests_properties(${test_name}_exists
    PROPERTIES
    FIXTURES_REQUIRED ${test_name}
    LABELS "${_lang_label};phase:check"
    PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
  )
  _salt_in_test_output(instrument ${test_name} ${test_name}_exists)
endfunction()

foreach(test_source IN LISTS TESTS_LIST)
  add_instrumentor_test(${test_source})
endforeach()

# Issue #53 regression: bodyless FunctionDecls must be skipped, not
# instrumented or crashed-on. Each check_<name>_skips_bodyless test
# reads the .inst output and verifies (a) main was instrumented
# (PASS_REGULAR_EXPRESSION) and (b) no TAU_PROFILE_TIMER mentions
# the symbols whose getBody() returns nullptr (FAIL_REGULAR_EXPRESSION).
# Symbol names are deliberately distinct from any text used in the
# fixture's comment block so the FAIL regex cannot false-positive on
# documentation strings.
add_test(NAME check_forward_decl_skips_bodyless
  COMMAND ${CMAKE_COMMAND} -E cat forward_decl.inst.c)
set_tests_properties(check_forward_decl_skips_bodyless
  PROPERTIES
  FIXTURES_REQUIRED instrument_forward_decl
  LABELS "lang:C;phase:check"
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
  FAIL_REGULAR_EXPRESSION "TAU_PROFILE_TIMER[^\n]*undefined_in_tu"
)

add_test(NAME check_pure_virtual_skips_bodyless
  COMMAND ${CMAKE_COMMAND} -E cat pure_virtual.inst.cpp)
set_tests_properties(check_pure_virtual_skips_bodyless
  PROPERTIES
  FIXTURES_REQUIRED instrument_pure_virtual
  LABELS "lang:CXX;phase:check"
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
  FAIL_REGULAR_EXPRESSION
    "TAU_PROFILE_TIMER[^\n]*Abstract::unimplemented;TAU_PROFILE_TIMER[^\n]*undefined_free_fn"
)

# --tau_instrument_inline tests
# Verify that inline functions are skipped without flag
# and are instrumented with it
add_test(NAME instrument_inline_off
  COMMAND
    ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
    --tau_output=instrument_inline_off.inst.cpp
    ${CMAKE_SOURCE_DIR}/tests/instrument_inline.cpp)
set_tests_properties(instrument_inline_off
  PROPERTIES
  REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/instrument_inline.cpp"
  FIXTURES_SETUP instrument_inline_off
  LABELS "lang:CXX;phase:instrument"
  PASS_REGULAR_EXPRESSION "[Ii]nstrumentation:"
)

add_test(NAME check_inline_off_skips_inline
  COMMAND ${CMAKE_COMMAND} -E cat instrument_inline_off.inst.cpp)
set_tests_properties(check_inline_off_skips_inline
  PROPERTIES
  FIXTURES_REQUIRED instrument_inline_off
  LABELS "lang:CXX;phase:check"
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
  FAIL_REGULAR_EXPRESSION
    "TAU_PROFILE_TIMER[^\n]*method_in_class_body;TAU_PROFILE_TIMER[^\n]*free_fn_marked_inline"
)

add_test(NAME instrument_inline_on
  COMMAND
    ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
    --tau_instrument_inline
    --tau_output=instrument_inline_on.inst.cpp
    ${CMAKE_SOURCE_DIR}/tests/instrument_inline.cpp)
set_tests_properties(instrument_inline_on
  PROPERTIES
  REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/instrument_inline.cpp"
  FIXTURES_SETUP instrument_inline_on
  LABELS "lang:CXX;phase:instrument"
  PASS_REGULAR_EXPRESSION "[Ii]nstrumentation:"
)

add_test(NAME check_inline_on_instruments_method
  COMMAND ${CMAKE_COMMAND} -E cat instrument_inline_on.inst.cpp)
set_tests_properties(check_inline_on_instruments_method
  PROPERTIES
  FIXTURES_REQUIRED instrument_inline_on
  LABELS "lang:CXX;phase:check"
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_TIMER[^\n]*method_in_class_body"
)

add_test(NAME check_inline_on_instruments_free_fn
  COMMAND ${CMAKE_COMMAND} -E cat instrument_inline_on.inst.cpp)
set_tests_properties(check_inline_on_instruments_free_fn
  PROPERTIES
  FIXTURES_REQUIRED instrument_inline_on
  LABELS "lang:CXX;phase:check"
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_TIMER[^\n]*free_fn_marked_inline"
)
# A source named relative to the working directory, with or without a
# leading `./`, gets its timers, and a header whose path contains the
# source's name adds none of its own.
foreach(kind dot bare)
  set(_src rel_path.c)
  if(kind STREQUAL "dot")
    set(_src ./rel_path.c)
  endif()
  set(_out ${SALT_TEST_OUTPUT_DIR}/instrument/rel_path_${kind}.inst.c)
  add_test(NAME instrument_rel_path_${kind}
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
            --tau_output=${_out} ${_src}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
  set_tests_properties(instrument_rel_path_${kind}
    PROPERTIES
    FIXTURES_REQUIRED instrument_dir
    FIXTURES_SETUP rel_path_${kind}
    LABELS "lang:C;phase:instrument"
    PASS_REGULAR_EXPRESSION "[Ii]nstrumentation:"
  )
  add_test(NAME check_rel_path_${kind} COMMAND ${CMAKE_COMMAND} -E cat ${_out})
  set_tests_properties(check_rel_path_${kind}
    PROPERTIES
    FIXTURES_REQUIRED "instrument_dir;rel_path_${kind}"
    LABELS "lang:C;phase:check"
    PASS_REGULAR_EXPRESSION "keep_routine[(]void[)] +\\[\\{"
    FAIL_REGULAR_EXPRESSION "header_routine[(]void[)] +\\[\\{"
  )
endforeach()
# A source whose path has braces, as the timer names around it do, gets
# its timers and main's setup code.
set(_src ${CMAKE_BINARY_DIR}/brace{d}.c)
configure_file(tests/sif_excl.c ${_src} COPYONLY)
set(_out ${SALT_TEST_OUTPUT_DIR}/instrument/brace_path.inst.c)
add_test(NAME instrument_brace_path
  COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --tau_output=${_out} ${_src})
set_tests_properties(instrument_brace_path
  PROPERTIES
  FIXTURES_REQUIRED instrument_dir
  FIXTURES_SETUP brace_path
  LABELS "lang:C;phase:instrument"
  PASS_REGULAR_EXPRESSION "[Ii]nstrumentation:"
)
add_test(NAME check_brace_path COMMAND ${CMAKE_COMMAND} -E cat ${_out})
set_tests_properties(check_brace_path
  PROPERTIES
  FIXTURES_REQUIRED "instrument_dir;brace_path"
  LABELS "lang:C;phase:check"
  PASS_REGULAR_EXPRESSION
    "TAU_PROFILE_TIMER[^\n]*keep_routine[^\n]*/brace[{]d[}][.]c[}] [{]3,.*TAU_PROFILE_SET_NODE"
)

_salt_in_test_output(instrument
  check_forward_decl_skips_bodyless check_pure_virtual_skips_bodyless
  instrument_inline_off check_inline_off_skips_inline
  instrument_inline_on check_inline_on_instruments_method
  check_inline_on_instruments_free_fn)

# saltfm must fail with an error naming its output file when it cannot
# write it. These failures hit root too, so the tests run in CI's
# root containers. The setup makes each default output name in
# test-output/output_error a directory, replacing any stray file, and
# removes `missing/`.
add_test(NAME make_output_error_paths
  COMMAND sh -c "rm -rf missing hello.inst.c c11test.inst.cpp hello.inst.F90 &&
    mkdir hello.inst.c c11test.inst.cpp hello.inst.F90")
set_tests_properties(make_output_error_paths PROPERTIES
  FIXTURES_SETUP output_error_paths
  LABELS "phase:setup")
_salt_in_test_output(output_error make_output_error_paths)
# Adds output_error_<name>: saltfm `ARGN` run in test-output/output_error
# must fail, naming `file`.
function(_salt_add_output_error_test name file label)
  add_test(NAME output_error_${name}
    COMMAND ${CMAKE_COMMAND} -DFILE=${file}
      -P ${CMAKE_SOURCE_DIR}/tests/expect_output_error.cmake --
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm ${ARGN})
  set_tests_properties(output_error_${name} PROPERTIES
    FIXTURES_REQUIRED output_error_paths
    LABELS "${label};phase:instrument")
  _salt_in_test_output(output_error output_error_${name})
endfunction()
# Adds output_error_isdir_<name>, where the output path is a directory,
# and output_error_nodir_<name>, where its directory does not exist.
function(_salt_add_output_error_tests name out label src)
  _salt_add_output_error_test(isdir_${name} ${out} ${label} ${src})
  _salt_add_output_error_test(nodir_${name} missing/${out} ${label}
    --tau_output=missing/${out} ${src})
endfunction()
_salt_add_output_error_tests(c hello.inst.c "lang:C"
  ${CMAKE_SOURCE_DIR}/tests/hello.c)
_salt_add_output_error_tests(cxx c11test.inst.cpp "lang:CXX"
  ${CMAKE_SOURCE_DIR}/tests/c11test.cpp)
# A full disk fails the writes, not the open.
if(EXISTS /dev/full)
  _salt_add_output_error_test(full_c /dev/full "lang:C"
    --tau_output=/dev/full ${CMAKE_SOURCE_DIR}/tests/hello.c)
endif()
# When Clang or Flang reports errors, saltfm must exit with a status
# from 1 to 127 and leave no file at the --tau_output path. The setup
# removes the `new`, fabricated, and missing header outputs, copies
# missing_header.c to the working directory, writes stale `old`,
# `early_exit`, and `forwarded_value` ones that saltfm must remove,
# makes the `dir` ones empty directories that saltfm must keep, copies
# the bad and good sources and a config and a selective instrumentation
# file to `self` ones that saltfm must refuse and keep when they are
# also the output, and makes read-only `ro` outputs that saltfm must
# not remove and then fail to open for writing.
set(_tests ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME make_clang_error_outputs
  COMMAND sh -c "rm -rf clang_error_new.inst.c fabricate_on.inst.cpp &&
    rm -f missing_header_fails.inst.c missing_header.inst.c &&
    rm -f allow_parse_errors_missing_input.inst.c &&
    rm -f allow_parse_errors_c.inst.c allow_parse_errors_env.inst.c &&
    rm -f allow_parse_errors_cxx.inst.cpp &&
    cp ${_tests}/missing_header.c missing_header.c &&
    rm -rf clang_error_dir.inst.c fortran_error_dir.inst.F90 &&
    mkdir clang_error_dir.inst.c fortran_error_dir.inst.F90 &&
    echo stale > clang_error_old.inst.c &&
    echo stale > fortran_error_old.inst.F90 &&
    echo stale > fortran_early_exit.inst.F90 &&
    echo stale > fortran_early_exit_space.inst.F90 &&
    rm -f clang_error_ro.inst.c fortran_error_ro.inst.F90 &&
    echo stale > clang_error_ro.inst.c &&
    echo stale > fortran_error_ro.inst.F90 &&
    chmod a-w clang_error_ro.inst.c fortran_error_ro.inst.F90 &&
    cp ${_tests}/clang_error.c clang_error_self.c &&
    cp ${_tests}/fortran/compile_error.f90 fortran_error_self.f90 &&
    cp ${_tests}/fortran/compile_error.f90 fortran_error_self.f95 &&
    cp ${_tests}/hello.c clang_hello_self.c &&
    cp ${_tests}/setup_timer_name.yaml clang_config_self.yaml &&
    cp ${_tests}/sif/exclude_c.tau clang_sif_self.tau &&
    cp ${_tests}/setup_timer_name.yaml fortran_config_self.yaml &&
    cp ${_tests}/setup_timer_name.yaml fortran_config_tail_self.yaml &&
    cp ${_tests}/sif/exclude_f90.tau fortran_sif_self.tau &&
    cp ${_tests}/fortran/hello.f90 fortran_hello_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_select_value_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_dash_x_name_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_other-x.f95 &&
    echo stale > fortran_forwarded_value")
set_tests_properties(make_clang_error_outputs PROPERTIES
  FIXTURES_SETUP clang_error_outputs
  LABELS "phase:setup")
_salt_in_test_output(output_error make_clang_error_outputs)
# Adds test `name`: saltfm run in test-output/output_error on `src`
# (relative to tests/) with --tau_output=`out`, the ARGS, and
# fabrication and parse error tolerance off unless ARGS turn them on,
# must exit with a status from 1 to 127 (0 with EXIT_ZERO) and pass
# `[ <CHECK> ]` (default: no `out`).
# NOT_ROOT skips the test as root, who can write read-only files.
# SPACE passes `--tau_output <out>` as two arguments, and DEFAULT_OUTPUT
# passes no --tau_output, so `out` is saltfm's default output. TAIL
# arguments follow `src`. Each LOG text must be in saltfm's output, and
# each OUT text in `out`.
# COPY_OF `fixture` (relative to tests/) instead expects the error that
# `out`, a copy of the fixture that is also `src` or another input, is
# an input file, and `out` unchanged.
function(_salt_add_clang_error_test name out labels src)
  cmake_parse_arguments(PARSE_ARGV 4 ARG "EXIT_ZERO;NOT_ROOT;SPACE;DEFAULT_OUTPUT"
    "CHECK;COPY_OF" "ARGS;TAIL;LOG;OUT")
  set(out_args --tau_output=${out})
  if(ARG_SPACE)
    set(out_args --tau_output ${out})
  elseif(ARG_DEFAULT_OUTPUT)
    set(out_args "")
  endif()
  cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
  set(check "[ ! -e ${out} ]")
  if(DEFINED ARG_CHECK)
    set(check "[ ${ARG_CHECK} ]")
  endif()
  if(DEFINED ARG_COPY_OF)
    set(check "cmp ${out} ${CMAKE_SOURCE_DIR}/tests/${ARG_COPY_OF}")
    list(APPEND ARG_LOG "ERROR: Output file ${out} is an input file")
  endif()
  set(text_checks "")
  set(log_file ${name}.log)
  set(out_file ${out})
  foreach(kind IN ITEMS log out)
    string(TOUPPER ${kind} keyword)
    foreach(text IN LISTS ARG_${keyword})
      string(APPEND text_checks "grep -qF -- \"${text}\" ${${kind}_file} ||
        { echo \"Expected ${text} in ${${kind}_file}\"; exit 1; }\n")
    endforeach()
  endforeach()
  set(rc_ok "[ $rc -ge 1 ] && [ $rc -le 127 ]")
  set(rc_want "from 1 to 127")
  if(ARG_EXIT_ZERO)
    set(rc_ok "[ $rc -eq 0 ]")
    set(rc_want "of 0")
  endif()
  set(skip "")
  if(ARG_NOT_ROOT)
    set(skip "[ \"$(id -u)\" -ne 0 ] || exit 77;")
  endif()
  add_test(NAME ${name}
    COMMAND sh -c "${skip} \"$0\" \"$@\" > ${name}.log 2>&1; rc=$?
      cat ${name}.log
      ${rc_ok} ||
        { echo \"Expected an exit status ${rc_want}, got $rc\"; exit 1; }
      ${check} || { echo \"Expected ${check}\"; exit 1; }
      ${text_checks}"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm ${ARG_ARGS}
      ${out_args} ${src} ${ARG_TAIL})
  set_tests_properties(${name} PROPERTIES
    REQUIRED_FILES "${src}"
    ENVIRONMENT "SALT_FABRICATE_UNKNOWN_TYPES=0;SALT_ALLOW_PARSE_ERRORS=0"
    FIXTURES_REQUIRED clang_error_outputs
    SKIP_RETURN_CODE 77
    LABELS "${labels};phase:instrument")
  _salt_in_test_output(output_error ${name})
endfunction()
foreach(out IN ITEMS new old)
  _salt_add_clang_error_test(clang_error_${out}_output
    clang_error_${out}.inst.c "lang:C" clang_error.c)
endforeach()
_salt_add_clang_error_test(clang_error_dir_output clang_error_dir.inst.c
  "lang:C" clang_error.c CHECK "-d clang_error_dir.inst.c")
_salt_add_clang_error_test(clang_error_self_output clang_error_self.c
  "lang:C" ${SALT_TEST_OUTPUT_DIR}/output_error/clang_error_self.c
  COPY_OF clang_error.c)
_salt_add_clang_error_test(clang_hello_self_output clang_hello_self.c
  "lang:C" ${SALT_TEST_OUTPUT_DIR}/output_error/clang_hello_self.c
  COPY_OF hello.c)
_salt_add_clang_error_test(clang_config_self_output clang_config_self.yaml
  "lang:C" hello.c COPY_OF setup_timer_name.yaml
  ARGS --config_file=clang_config_self.yaml)
_salt_add_clang_error_test(clang_sif_self_output clang_sif_self.tau
  "lang:C" hello.c COPY_OF sif/exclude_c.tau
  ARGS --tau_select_file=clang_sif_self.tau)
# saltfm does not remove a read-only output, so opening it for writing
# fails even for a good source.
_salt_add_clang_error_test(clang_error_ro_output clang_error_ro.inst.c
  "lang:C" hello.c NOT_ROOT
  CHECK "-f clang_error_ro.inst.c -a ! -w clang_error_ro.inst.c")
# Run, as when instrumenting outside the build, with no -I for the
# missing header: without --tau_allow_parse_errors, saltfm fails; with
# it, it warns and instruments the partial parse tree Clang builds.
_salt_add_clang_error_test(missing_header_fails missing_header_fails.inst.c
  "lang:C" missing_header.c LOG "'grid_c.h' file not found")
set(_allow_warning
  "WARNING: Clang reported errors; instrumenting the partial parse tree")
# Without fabrication, Clang shows the unknown types as `int`.
set(_c_timers "void scale(int, int *)" "int count(int)")
_salt_add_clang_error_test(allow_parse_errors_c allow_parse_errors_c.inst.c
  "lang:C" missing_header.c EXIT_ZERO CHECK "-s allow_parse_errors_c.inst.c"
  ARGS --tau_allow_parse_errors LOG "${_allow_warning}" OUT ${_c_timers})
_salt_add_clang_error_test(allow_parse_errors_env
  allow_parse_errors_env.inst.c "lang:C" missing_header.c EXIT_ZERO
  CHECK "-s allow_parse_errors_env.inst.c"
  LOG "${_allow_warning}" OUT ${_c_timers})
set_property(TEST allow_parse_errors_env APPEND
  PROPERTY ENVIRONMENT "SALT_ALLOW_PARSE_ERRORS=1")
# As `cd <dir>; saltfm <file>` does: the default output in the source's
# directory.
_salt_add_clang_error_test(allow_parse_errors_default_output
  missing_header.inst.c "lang:C"
  ${SALT_TEST_OUTPUT_DIR}/output_error/missing_header.c DEFAULT_OUTPUT
  EXIT_ZERO CHECK "-s missing_header.inst.c"
  ARGS --tau_allow_parse_errors LOG "${_allow_warning}" OUT ${_c_timers})
_salt_add_clang_error_test(allow_parse_errors_cxx_fabricate
  allow_parse_errors_cxx.inst.cpp "lang:CXX;fabricate" missing_header.cpp
  EXIT_ZERO CHECK "-s allow_parse_errors_cxx.inst.cpp"
  ARGS --tau_allow_parse_errors --tau_fabricate_unknown_types
  LOG "${_allow_warning}"
  OUT "scale(RealD, ComplexD &)" "int count(int)"
  "computeOmega(std::vector<ComplexD> &, const int, const std::vector<RealD> &, const int)")
# A source that does not exist gives nothing to instrument, so saltfm
# must fail and leave no output for TAU's compiler wrapper to compile.
_salt_add_clang_error_test(allow_parse_errors_missing_input
  allow_parse_errors_missing_input.inst.c "lang:C" no_such_source.c
  ARGS --tau_allow_parse_errors LOG "ERROR: Could not open input file")
set_property(TEST allow_parse_errors_missing_input PROPERTY REQUIRED_FILES "")

# Selective Instrumentation File (SIF) tests.
#
# Each test runs the SALT instrumentor against a small source file containing
# two routines (keep_routine and excluded_routine) under a SIF that exercises
# one of the supported list types, then inspects the instrumented output for
# (or absence of) TAU_PROFILE_TIMER markers.
#
# Timer string format markers:
#   C/C++: "<return-type> <name>(<params>)  [{<file>}..."
#   Fortran (continued line): "&<name> [{<file>}..."
#
# Each test passes --tau_output so parallel ctest invocations don't clobber a
# shared default output path (saltfm writes to <basename>.inst.<ext> by default).

# Sets <test>'s regexes so its output holds exactly the SIF list-entry
# warnings in `warnings`: one regex per expected warning, in log order;
# each must match within one warning line and no two may match the same
# line. With no `warnings`, any list-entry warning fails. Only the
# list-entry wording is counted, since the Fortran plugin's verbose
# output has unrelated "WARNING:" lines. `fail` holds other
# FAIL_REGULAR_EXPRESSION entries; all are OR'd with the count.
function(_sif_set_warning_checks test warnings fail)
  # Fail on one list-entry warning more than expected and require the
  # expected ones.
  set(entry_warning "WARNING: [^\n]*selective instrumentation file line")
  set(too_many "${entry_warning}")
  foreach(expected_warning IN LISTS warnings)
    string(APPEND too_many ".*${entry_warning}")
  endforeach()
  list(APPEND fail "${too_many}")
  set_tests_properties(${test} PROPERTIES FAIL_REGULAR_EXPRESSION "${fail}")
  if(NOT warnings STREQUAL "")
    list(JOIN warnings ".*" expected_warnings)
    set_tests_properties(${test} PROPERTIES PASS_REGULAR_EXPRESSION "${expected_warnings}")
  endif()
endfunction()

# Standard SIF test: instrumented output IS produced and inspected by
# check_sif_<test_name>. The caller is responsible for setting
# PASS_REGULAR_EXPRESSION / FAIL_REGULAR_EXPRESSION on the check test.
# warn_sif_<test_name> checks the instrumentor's SIF list-entry warnings
# against the optional WARNINGS (see _sif_set_warning_checks). The optional
# CONFIG names a configuration file relative to the source directory, and
# the optional ARGS are extra instrumentor options.
function(add_sif_test test_name lang src sif)
  cmake_parse_arguments(PARSE_ARGV 4 ARG "" "CONFIG" "WARNINGS;ARGS")
  set(name sif_${test_name})
  set(config_arg "")
  if(ARG_CONFIG)
    set(config_arg --config_file=${CMAKE_SOURCE_DIR}/${ARG_CONFIG})
  endif()
  get_filename_component(sif_path ${sif} ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})
  get_filename_component(_src_ext ${src} EXT)
  set(out_file ${name}.inst${_src_ext})
  set(log_file ${name}.log)
  _salt_lang_to_label(${lang} _lang_label)
  # Drop output from an earlier run so the check step cannot pass on
  # stale results when the instrument step writes nothing.
  add_test(NAME rm_old_${name}_output
    COMMAND ${CMAKE_COMMAND} -E rm -f "${out_file}"
  )
  set_tests_properties(rm_old_${name}_output
    PROPERTIES
    FIXTURES_SETUP ${name}_output
    LABELS "${_lang_label};phase:setup;sif"
  )
  # Save the instrumentor's output for warn_${name} (`>` truncates any
  # old log), then echo it so this step still matches its own pass
  # pattern.
  add_test(NAME instrument_${name}
    COMMAND sh -c "\"$0\" \"$@\" > ${log_file} 2>&1; rc=$?; cat ${log_file}; exit $rc"
            "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm"
            ${config_arg}
            ${ARG_ARGS}
            --tau_select_file=${sif_path}
            --tau_output=${out_file}
            ${CMAKE_SOURCE_DIR}/${src}
  )
  if(lang STREQUAL "fortran")
    set(instrument_pass_pattern "SALT Instrumentor Plugin finished")
  else()
    set(instrument_pass_pattern "[Ii]nstrumentation:")
  endif()
  set_tests_properties(instrument_${name}
    PROPERTIES
    REQUIRED_FILES "${CMAKE_SOURCE_DIR}/${src};${sif_path}"
    ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
    FIXTURES_REQUIRED ${name}_output
    FIXTURES_SETUP ${name}_inst
    LABELS "${_lang_label};phase:instrument;sif"
    PASS_REGULAR_EXPRESSION "${instrument_pass_pattern}"
  )
  add_test(NAME check_${name}
    COMMAND ${CMAKE_COMMAND} -E cat ./${out_file}
  )
  # Warnings are checked apart from the output, so a mismatch in one does
  # not hide the other.
  add_test(NAME warn_${name}
    COMMAND ${CMAKE_COMMAND} -E cat "./${log_file}"
  )
  _sif_set_warning_checks(warn_${name} "${ARG_WARNINGS}" "")
  # Fixtures (not DEPENDS) so `ctest -R check_...` alone also runs the
  # rm and instrument steps first.
  set_tests_properties(check_${name} warn_${name}
    PROPERTIES
    FIXTURES_REQUIRED "${name}_output;${name}_inst"
    LABELS "${_lang_label};phase:check;sif"
  )
  _salt_in_test_output(sif
    rm_old_${name}_output instrument_${name} check_${name} warn_${name})
endfunction()

# SIF test where the instrumentor is expected NOT to instrument the source
# (e.g. the source is on a FILE_EXCLUDE_LIST or fails the FILE_INCLUDE_LIST
# whitelist). Verified by the absence of cparse-llvm's "Instrumentation:"
# banner, and check_sif_<test_name>_copy checks that the output is the
# source after a `#line 1` line, so that TAU's compiler wrapper compiles
# the source as written. Pass --tau_output to a per-test unique path so parallel
# ctest runs cannot race on a shared default location. The instrument step
# checks the SIF list-entry warnings against the optional WARNINGS, and
# the optional ARGS are extra instrumentor options, as in add_sif_test.
function(add_sif_test_no_output test_name lang src sif)
  cmake_parse_arguments(PARSE_ARGV 4 ARG "" "" "WARNINGS;ARGS")
  set(name sif_${test_name})
  get_filename_component(_src_ext ${src} EXT)
  set(out_file ${name}.inst${_src_ext})
  _salt_lang_to_label(${lang} _lang_label)
  add_test(NAME rm_old_${name}_output
    COMMAND ${CMAKE_COMMAND} -E rm -f "${out_file}"
  )
  set_tests_properties(rm_old_${name}_output
    PROPERTIES
    FIXTURES_SETUP ${name}_output
    LABELS "${_lang_label};phase:setup;sif"
  )
  add_test(NAME instrument_${name}
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
            ${ARG_ARGS}
            --tau_select_file=${CMAKE_SOURCE_DIR}/${sif}
            --tau_output=${out_file}
            ${CMAKE_SOURCE_DIR}/${src}
  )
  set_tests_properties(instrument_${name}
    PROPERTIES
    REQUIRED_FILES "${CMAKE_SOURCE_DIR}/${src};${CMAKE_SOURCE_DIR}/${sif}"
    FIXTURES_REQUIRED ${name}_output
    FIXTURES_SETUP ${name}_inst
    LABELS "${_lang_label};phase:instrument;sif"
  )
  _sif_set_warning_checks(instrument_${name} "${ARG_WARNINGS}" "Instrumentation:")
  add_test(NAME check_${name}_copy
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/${src} -DOUT=${out_file}
            -P ${CMAKE_SOURCE_DIR}/tests/check_copy.cmake
  )
  set_tests_properties(check_${name}_copy
    PROPERTIES
    FIXTURES_REQUIRED "${name}_output;${name}_inst"
    LABELS "${_lang_label};phase:check;sif"
  )
  _salt_in_test_output(sif
    rm_old_${name}_output instrument_${name} check_${name}_copy)
endfunction()

# Pattern helpers: regex fragments that match a TAU_PROFILE_TIMER timer string
# referencing a named routine. The C/C++ matcher emits the routine signature
# inline; the Fortran matcher emits a continuation line with the bare name.
# The Fortran patterns spell the '[' of '[{' as '.', since CMake does not
# split a list at a ';' that follows an unbalanced '['.
if(TEST_FORTRAN)
  set(_sif_keep_pattern_fortran    "keep_routine .[{]")
  set(_sif_excluded_pattern_fortran "excluded_routine .[{]")
endif()
set(_sif_keep_pattern_c     "keep_routine[(]void[)] +\\[\\{")
set(_sif_keep_pattern_cpp   "${_sif_keep_pattern_c}")
set(_sif_excluded_pattern_c "excluded_routine[(]void[)] +\\[\\{")
set(_sif_excluded_pattern_cpp "${_sif_excluded_pattern_c}")

# Routine-level EXCLUDE_LIST: blacklist excluded_routine.
add_sif_test(exclusion_c   c   tests/sif_excl.c   tests/sif/exclude_c.tau)
add_sif_test(exclusion_cpp cpp tests/sif_excl.cpp tests/sif/exclude_cpp.tau)
foreach(lang c cpp)
  set_tests_properties(check_sif_exclusion_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_${lang}}"
  )
endforeach()

# Issue #64: as in TAU and PDT, a leading '#' starts a comment even inside
# a BEGIN_/END_ list block; quoting ("#noinst#") is how a pattern starts
# with the '#' wildcard. The unquoted reproducer from the issue (and
# "##noinst", "#*noinst", "#@noinst", non-ASCII "#énoinst") is ignored,
# so both routines keep their timers, but because each line looks like a
# pattern the instrumentor must warn and suggest quoting it, once per line.
string(CONCAT _sif_hash_comment_warning
  "WARNING: '#noinst#' at selective instrumentation file line 7 treated "
  "as a comment in exclude list[.] Quote it [(]\"#noinst#\"[)] to use it "
  "as a wildcard")
add_sif_test(exclusion_unquoted_hash_comment_cpp
  cpp tests/sif_excl_wildcard_prefix.cpp tests/sif/exclude_unquoted_hash_comment_cpp.tau
  WARNINGS "${_sif_hash_comment_warning}"
    "'##noinst' at selective instrumentation file line 8 treated as a comment"
    "'#[*]noinst' at selective instrumentation file line 9 treated as a comment"
    "'#@noinst' at selective instrumentation file line 10 treated as a comment"
    "'#énoinst' at selective instrumentation file line 11 treated as a comment")
# Single regex spanning both timers (source order): a list here would be
# OR'd, not AND'd. CMake's '.' matches newlines.
set_tests_properties(check_sif_exclusion_unquoted_hash_comment_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "dosomething_inst[(]void[)] +\\[\\{.*dosomething_noinst[(]void[)] +\\[\\{"
)

# No warning at all for ordinary comments in a routine list ('#' then a
# space or tab, a lone '#', the '#=====' ruler and '#' followed by each of
# the 14 quiet punctuation characters), '#' lines in FILE lists (whose
# wildcard is '*') or whitespace after a closing quote. The quoted
# "#noinst#" entry deliberately ends in trailing spaces, which are not
# text after the closing quote; it still excludes dosomething_noinst.
# `.gitattributes` and `.editorconfig` stop git and EditorConfig editors
# from trimming that whitespace; fixture_sif_exclusion_no_warning_cpp
# fails if any other tool strips it.
add_sif_test(exclusion_no_warning_cpp
  cpp tests/sif_excl_wildcard_prefix.cpp tests/sif/exclude_no_warning_cpp.tau)
# This C++ log has no unrelated warnings, so fail on any "WARNING", not
# just list-entry ones (e.g. "Instrument section is not supported yet").
set_tests_properties(warn_sif_exclusion_no_warning_cpp
  PROPERTIES FAIL_REGULAR_EXPRESSION "WARNING")
set_tests_properties(check_sif_exclusion_no_warning_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "dosomething_inst[(]void[)] +\\[\\{"
  FAIL_REGULAR_EXPRESSION "dosomething_noinst[(]void[)] +\\[\\{"
)
# One regex, since a list would be OR'd: the tab line, then "#noinst#"
# followed by exactly three spaces.
add_test(NAME fixture_sif_exclusion_no_warning_cpp
  COMMAND ${CMAKE_COMMAND} -E cat
          "${CMAKE_SOURCE_DIR}/tests/sif/exclude_no_warning_cpp.tau"
)
set_tests_properties(fixture_sif_exclusion_no_warning_cpp
  PROPERTIES
  LABELS "lang:CXX;phase:check;sif"
  PASS_REGULAR_EXPRESSION "\n#\tcomment\n.*\n\"#noinst#\"   \n"
)

# Suspicious quoting in routine list entries must produce a stderr warning
# while parsing stays lenient. The warn step asserts the warning text; the
# check step asserts the lenient parse result.
# - unclosed quote: pattern = rest of line, still excludes noinst
# - text after closing quote: dropped, pattern still excludes noinst
# - lone quote: empty pattern, matches nothing, both routines keep timers;
#   one warning only (empty pattern, not also a missing closing quote)
# File lists do not remove quotes; see file_exclude_quoted below.
add_sif_test(exclusion_unclosed_quote_cpp
  cpp tests/sif_excl_wildcard_prefix.cpp tests/sif/exclude_unclosed_quote_cpp.tau
  WARNINGS "WARNING: missing closing quote in exclude list entry")
set_tests_properties(check_sif_exclusion_unclosed_quote_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "dosomething_inst[(]void[)] +\\[\\{"
  FAIL_REGULAR_EXPRESSION "dosomething_noinst[(]void[)] +\\[\\{"
)

add_sif_test(exclusion_quote_trailing_junk_cpp
  cpp tests/sif_excl_wildcard_prefix.cpp tests/sif/exclude_quote_trailing_junk_cpp.tau
  WARNINGS "WARNING: ignoring text after closing quote in exclude list entry")
set_tests_properties(check_sif_exclusion_quote_trailing_junk_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "dosomething_inst[(]void[)] +\\[\\{"
  FAIL_REGULAR_EXPRESSION "dosomething_noinst[(]void[)] +\\[\\{"
)

add_sif_test(exclusion_empty_quote_cpp
  cpp tests/sif_excl_wildcard_prefix.cpp tests/sif/exclude_empty_quote_cpp.tau
  WARNINGS "WARNING: empty quoted exclude list entry")
# Single regex spanning both timers (source order): a list here would be
# OR'd, not AND'd. CMake's '.' matches newlines.
set_tests_properties(check_sif_exclusion_empty_quote_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "dosomething_inst[(]void[)] +\\[\\{.*dosomething_noinst[(]void[)] +\\[\\{"
)

# Checks that the output of SIF test `test_name`, whose file lists exclude
# its source, has main's TAU setup code but no timer (#71).
function(_sif_check_file_excluded test_name)
  set_tests_properties(check_sif_${test_name}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
    FAIL_REGULAR_EXPRESSION "TAU_PROFILE_TIMER"
  )
endfunction()

# File-level EXCLUDE_LIST: the C/C++ frontend does not instrument a source
# file on the file exclude list. At the `always` level, main keeps TAU's
# setup code, and a file without main is copied.
foreach(lang c cpp)
  add_sif_test(file_exclude_${lang}
    ${lang} tests/sif_excl.${lang} tests/sif/file_exclude_${lang}.tau
    ARGS --tau_main_setup_parse_level=always)
  _sif_check_file_excluded(file_exclude_${lang})
endforeach()
add_sif_test_no_output(file_exclude_nomain_c
  c tests/sif_nomain.c tests/sif/file_exclude_nomain_c.tau)
# An excluded empty source gets an empty copy, not a write error.
add_sif_test_no_output(file_exclude_empty_c c tests/sif_empty.c tests/sif/file_exclude_empty_c.tau)

# FILE_INCLUDE_LIST: cover both branches of the whitelist gate.
# - match: source IS on the list, so instrumentation proceeds and
#   keep_routine appears in the output.
# - nomatch: list contains a real but unrelated file, so the source under
#   test is not whitelisted and its output has no timer.
# Together these protect against both "list silently ignored" and
# "match condition silently over-broad" parser regressions.
add_sif_test(file_include_match_c
  c   tests/sif_excl.c   tests/sif/file_include_match_c.tau)
add_sif_test(file_include_match_cpp
  cpp tests/sif_excl.cpp tests/sif/file_include_match_cpp.tau)
foreach(lang c cpp)
  set_tests_properties(check_sif_file_include_match_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
  )
endforeach()
foreach(lang c cpp)
  add_sif_test(file_include_nomatch_${lang}
    ${lang} tests/sif_excl.${lang} tests/sif/file_include_nomatch_${lang}.tau)
  _sif_check_file_excluded(file_include_nomatch_${lang})
endforeach()

if(TEST_FORTRAN)
  add_sif_test(exclusion_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/exclude_f90.tau)
  set_tests_properties(check_sif_exclusion_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )
  # A configuration file without the optional program_setup_insert still
  # instruments the main program from its program_insert.
  add_sif_test(no_program_setup_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/exclude_f90.tau
    CONFIG tests/fortran/no_program_setup.yaml)
  set_tests_properties(check_sif_no_program_setup_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION
      "TAU_PROFILE_INIT.*[Ss][Ii][Ff]_[Ee][Xx][Cc][Ll] .[{].*${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )

  # Routine-level INCLUDE_LIST (whitelist): only keep_routine should be
  # instrumented; excluded_routine, omitted from the include list, should not.
  # The C/C++ twins are include_list_c and include_list_cpp.
  add_sif_test(inclusion_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/include_routine_f90.tau)
  set_tests_properties(check_sif_inclusion_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )

  add_sif_test(file_exclude_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_exclude_f90.tau)
  _sif_check_file_excluded(file_exclude_fortran)

  add_sif_test(file_include_match_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_match_f90.tau)
  set_tests_properties(check_sif_file_include_match_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
  )
  add_sif_test(file_include_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_nomatch_f90.tau)
  _sif_check_file_excluded(file_include_nomatch_fortran)

  # Regression guards for the Fortran matcher's full-string semantics.
  # The flang plugin previously used regex_search (substring match), which
  # caused bare entries like "routine" or "excl" to silently match more
  # subprograms or files than intended. See issue #47.

  # A SIF entry that is only a substring of subprogram names must NOT
  # exclude them. Both keep_routine and excluded_routine should remain
  # instrumented after applying the SIF.
  add_sif_test(routine_substring_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/exclude_substring_routine_f90.tau)
  set_tests_properties(check_sif_routine_substring_nomatch_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )

  # Mirror of the EXCLUDE substring guard for the INCLUDE side: a bare
  # entry "routine" must NOT whitelist keep_routine or excluded_routine
  # via substring matching. With full-string semantics neither subprogram
  # is on the whitelist, so neither timer should appear.
  add_sif_test(routine_substring_include_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/include_substring_routine_f90.tau)
  set_tests_properties(check_sif_routine_substring_include_nomatch_fortran
    PROPERTIES
    FAIL_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran};${_sif_excluded_pattern_fortran}"
  )

  # A FILE_EXCLUDE entry that is only a substring of the source basename
  # must NOT exclude the file. Subprogram timers must still be emitted.
  add_sif_test(file_exclude_substring_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_exclude_substring_f90.tau)
  set_tests_properties(check_sif_file_exclude_substring_nomatch_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
  )

  # A FILE_INCLUDE entry that is only a substring of the source basename
  # must NOT whitelist the file, so no TAU timers should appear.
  add_sif_test(file_include_substring_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_substring_f90.tau)
  set_tests_properties(check_sif_file_include_substring_nomatch_fortran
    PROPERTIES
    FAIL_REGULAR_EXPRESSION "TAU_PROFILE_TIMER"
  )

  # FILE_INCLUDE_LIST with an actual `*` glob. The basename "sif_excl.f90"
  # matches "sif_*.f90", so the file is whitelisted and keep_routine is
  # instrumented. Exercises the `*` wildcard in file matching, separate
  # from the literal-basename file_include_match test.
  add_sif_test(file_include_glob_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_glob_f90.tau)
  set_tests_properties(check_sif_file_include_glob_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
  )

  # FILE_EXCLUDE_LIST with an actual `*` glob. The basename "sif_excl.f90"
  # matches "sif_*.f90", so the file is excluded and no TAU timers are
  # emitted. Mirror of file_include_glob_fortran for the exclude path.
  add_sif_test(file_exclude_glob_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_exclude_glob_f90.tau)
  set_tests_properties(check_sif_file_exclude_glob_fortran
    PROPERTIES
    FAIL_REGULAR_EXPRESSION "TAU_PROFILE_TIMER"
  )

  # Wildcard sanity check: a "#" kleene-star pattern must still match the
  # subprograms whose suffix it captures, so neither keep_routine nor
  # excluded_routine should appear with a TAU timer (only main remains).
  add_sif_test(routine_wildcard_excludes_both_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/exclude_wildcard_routine_f90.tau)
  set_tests_properties(check_sif_routine_wildcard_excludes_both_fortran
    PROPERTIES
    FAIL_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran};${_sif_excluded_pattern_fortran}"
  )

  # Unquoted twin of the wildcard sanity check above (issue #64): without
  # quotes the leading '#' starts a comment, so nothing is excluded and
  # both subprograms keep their timers (single regex in source order, since
  # a list would be OR'd). The instrumentor must warn about the line.
  add_sif_test(routine_unquoted_hash_comment_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/exclude_unquoted_hash_comment_f90.tau
    WARNINGS "'#routine' at [^\n]* line 6 treated as a comment in exclude list")
  set_tests_properties(check_sif_routine_unquoted_hash_comment_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}.*${_sif_excluded_pattern_fortran}"
  )

  # Combined INCLUDE + EXCLUDE: per the TAU SIF spec the exclude list
  # operates on whatever survives the include filter. The SIF below
  # whitelists "#routine" (matches both subprograms) and then explicitly
  # excludes excluded_routine. Only keep_routine should retain its timer.
  add_sif_test(include_wildcard_exclude_specific_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/include_wildcard_exclude_specific_f90.tau)
  set_tests_properties(check_sif_include_wildcard_exclude_specific_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )
  # Include-list twin of routine_unquoted_hash_comment_fortran: unquoted
  # "#keep_routine" is a comment, the include list stays empty and both
  # subprograms keep their timers (quoted, only keep_routine would).
  add_sif_test(include_unquoted_hash_comment_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/include_unquoted_hash_comment_f90.tau
    WARNINGS "'#keep_routine' at [^\n]* line 7 treated as a comment in include list")
  set_tests_properties(check_sif_include_unquoted_hash_comment_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}.*${_sif_excluded_pattern_fortran}"
  )

  # Fortran routine names ignore case in the include and exclude lists.
  add_sif_test(routine_case_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/routine_case_f90.tau)
  set_tests_properties(check_sif_routine_case_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_fortran}"
  )
endif()

# SIF tests shared by the C, C++ and Fortran instrumentors. Each SIF works
# with any of the sif_excl sources.
set(_sif_langs c cpp)
set(_sif_src_c tests/sif_excl.c)
set(_sif_src_cpp tests/sif_excl.cpp)
if(TEST_FORTRAN)
  list(APPEND _sif_langs fortran)
  set(_sif_src_fortran tests/fortran/sif_excl.f90)
endif()

# Whitespace at the two ends of each SIF line, and the '\r' of a CRLF
# line, is removed before the BEGIN_/END_ tokens are compared and before
# an entry goes into a list. Each SIF excludes excluded_routine and keeps
# the source through a file include list.
foreach(case trailing_whitespace crlf)
  foreach(lang IN LISTS _sif_langs)
    add_sif_test(${case}_${lang} ${lang} ${_sif_src_${lang}} tests/sif/${case}.tau)
    set_tests_properties(check_sif_${case}_${lang}
      PROPERTIES
      PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
      FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_${lang}}"
    )
  endforeach()
endforeach()

# One wildcard matcher for routines and files, in C, C++ and Fortran:
# - question_mark: '?' matches one character in a routine and a file
#   pattern.
# - file_path: a file pattern with a '/' matches the whole path.
# - metachars: regular expression metacharacters ('.', '[', '$') are
#   literal, so both routines keep their timers. Fortran escaped them
#   before the shared matcher, so it is only tested for C and C++.
foreach(case question_mark file_path)
  foreach(lang IN LISTS _sif_langs)
    add_sif_test(${case}_${lang} ${lang} ${_sif_src_${lang}} tests/sif/${case}.tau)
    set_tests_properties(check_sif_${case}_${lang}
      PROPERTIES
      PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
      FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_${lang}}"
    )
  endforeach()
endforeach()
foreach(lang c cpp)
  add_sif_test(metachars_${lang} ${lang} ${_sif_src_${lang}} tests/sif/metachars.tau)
  set_tests_properties(check_sif_metachars_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION
      "${_sif_keep_pattern_${lang}}.*${_sif_excluded_pattern_${lang}}"
  )
endforeach()

# The name a C++ routine entry matches ends before the timer's " [{"
# source location, not at the first '[', which operator[] has.
add_sif_test(operator_index_cpp cpp tests/sif_operator.cpp tests/sif/operator_index.tau)
set_tests_properties(check_sif_operator_index_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "int main[(]void[)] +\\[\\{"
  FAIL_REGULAR_EXPRESSION "operator\\[\\][(]int[)] +\\[\\{"
)

# Routine-level INCLUDE_LIST for C and C++ (#46): only keep_routine is
# instrumented, and main and excluded_routine get no timer. main keeps
# TAU's setup code (TAU_INIT and TAU_PROFILE_SET_NODE(0)), though (#71).
foreach(lang c cpp)
  add_sif_test(include_list_${lang} ${lang} ${_sif_src_${lang}} tests/sif/include_list.tau)
  set_tests_properties(check_sif_include_list_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}.*TAU_PROFILE_SET_NODE"
    FAIL_REGULAR_EXPRESSION
      "TAU_PROFILE_TIMER[^\n]*excluded_routine;TAU_PROFILE_TIMER[^\n]*int main"
  )
endforeach()
if(TEST_FORTRAN)
  # '.[{]' stands for the '[{' after a timer name: CMake does not split a
  # list at a ';' that follows an unbalanced '['. Flang may spell the
  # program name in capitals.
  set(_sif_main_timer_fortran "[Ss][Ii][Ff]_[Ee][Xx][Cc][Ll] .[{]")
  add_sif_test(include_list_fortran fortran tests/fortran/sif_excl.f90 tests/sif/include_list.tau)
  set_tests_properties(check_sif_include_list_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE.*${_sif_keep_pattern_fortran}"
    FAIL_REGULAR_EXPRESSION "excluded_routine .[{];${_sif_main_timer_fortran}"
  )
endif()

# A SIF that excludes the main program removes only its timer; main
# keeps TAU's setup code (#71).
set(_sif_main_timer_c "TAU_PROFILE_TIMER[^\n]*int main")
set(_sif_main_timer_cpp "${_sif_main_timer_c}")
foreach(lang IN LISTS _sif_langs)
  add_sif_test(main_excluded_${lang} ${lang} ${_sif_src_${lang}} tests/sif/exclude_main.tau)
  set_tests_properties(check_sif_main_excluded_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
    FAIL_REGULAR_EXPRESSION "${_sif_main_timer_${lang}}"
  )
endforeach()
# The main of sif_excl has no parameters, so it gets
# main_setup_insert_noargs, without TAU_INIT(&argc, &argv).
foreach(lang c cpp)
  set_property(TEST check_sif_main_excluded_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "TAU_INIT[(]&argc")
endforeach()

# Setup code fills in main's timer name, as for a kept main, when a
# routine or file list removes main's timer.
set(_sif_setup_name_c "SETUP_TIMER_NAME[(]\"int main[(]")
set(_sif_setup_name_cpp "${_sif_setup_name_c}")
set(_sif_setup_name_fortran "SETUP_TIMER_NAME[(]\"&\n     &${_sif_main_timer_fortran}")
set(_sif_file_exclude_c file_exclude_c)
set(_sif_file_exclude_cpp file_exclude_cpp)
set(_sif_file_exclude_fortran file_exclude_f90)
foreach(lang IN LISTS _sif_langs)
  foreach(sif exclude_main include_list ${_sif_file_exclude_${lang}})
    add_sif_test(setup_name_${sif}_${lang} ${lang} ${_sif_src_${lang}} tests/sif/${sif}.tau
      CONFIG tests/setup_timer_name.yaml)
    set_tests_properties(check_sif_setup_name_${sif}_${lang}
      PROPERTIES
      PASS_REGULAR_EXPRESSION "${_sif_setup_name_${lang}}"
      FAIL_REGULAR_EXPRESSION "[$][{]full_timer_name")
  endforeach()
endforeach()

# The perfstubs and ITT configurations also keep their setup code.
foreach(cfg perfstubs itt)
  add_sif_test(main_excluded_${cfg}_c c tests/sif_excl.c tests/sif/exclude_main.tau
    CONFIG config_files/${cfg}_config.yaml)
  set_tests_properties(check_sif_main_excluded_${cfg}_c
    PROPERTIES
    PASS_REGULAR_EXPRESSION "PERFSTUBS_INITIALIZE"
    FAIL_REGULAR_EXPRESSION "PERFSTUBS_INITIALIZE[(][)].\nPERFSTUBS_TIMER_START_FUNC")
endforeach()

# Routines that the lists leave out keep returns that share their line with
# other code as written: no repeated code before the return, and no timer
# stop in main. '.' stands for each ';' of the source, which would split
# the pattern lists.
string(CONCAT _sif_oneline_pass
  "\nint dropped[(]void[)] [{] return 11. [}]\n"
  ".*\n    if [(]n == 22[)] return 22.\n"
  ".* return dropped[(][)] [+] cond_dropped[(]0[)] [+] kept[(][)] - 44. [}]\n")
foreach(sif include exclude file_exclude)
  add_sif_test(oneline_${sif} c tests/sif_oneline.c tests/sif/oneline_${sif}.tau)
  set_tests_properties(check_sif_oneline_${sif}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_oneline_pass}"
    FAIL_REGULAR_EXPRESSION
      "int dropped.*int dropped;n == 22.*n == 22;int main.*int main;inst_ret_val = dropped"
  )
endforeach()
# The same with the C++ API, which TAU's compiler wrapper uses for C++,
# and where a one-line routine the lists leave out lost the rest of its
# line.
add_sif_test(oneline_exclude_cpp cpp tests/sif_oneline.cpp tests/sif/oneline_exclude_cpp.tau
  ARGS --tau_use_cxx_api)
string(CONCAT _sif_oneline_pass
  "\nint dropped[(][)] [{] return 11. [}]\n"
  "\nvoid vdrop[(]int n[)] [{] if [(]n[)] return. g = 1. [}]\n"
  ".*TAU_PROFILE[(]\"int kept[(]"
  ".* vdrop[(]0[)]. return dropped[(][)] [+] kept[(][)] - 44 [+] g. [}]\n")
set_tests_properties(check_sif_oneline_exclude_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "${_sif_oneline_pass}"
  FAIL_REGULAR_EXPRESSION "int dropped.*int dropped;vdrop[(]int.*vdrop[(]int;int main.*int main"
)

# An include list that leaves out the main program gets a warning that
# main still gets TAU's setup code (#71); an exclude list entry for main
# silences it.
set(_sif_main_warning
  "WARNING: main program \"[^\"\n]*\" is not on the selective\n  instrumentation include list")
foreach(lang IN LISTS _sif_langs)
  set_tests_properties(warn_sif_include_list_${lang}
    PROPERTIES PASS_REGULAR_EXPRESSION "${_sif_main_warning}")
  add_sif_test(include_exclude_main_${lang} ${lang} ${_sif_src_${lang}}
    tests/sif/include_exclude_main.tau)
  set_tests_properties(check_sif_include_exclude_main_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE"
    FAIL_REGULAR_EXPRESSION "${_sif_main_timer_${lang}}"
  )
  set_property(TEST warn_sif_include_exclude_main_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "${_sif_main_warning}")
endforeach()

# Quotes are part of a file pattern, as in TAU, so a quoted file entry does
# not match the source, and SALT-FM warns about it. Routine lists still
# remove quotes.
string(CONCAT _sif_quoted_file_warning
  "WARNING: file %s list entry at selective instrumentation file line 4:\n"
  "  the quotes in \"sif_excl[.][*]\" are part of the pattern")
foreach(lang IN LISTS _sif_langs)
  string(REPLACE "%s" "exclude" _warning "${_sif_quoted_file_warning}")
  add_sif_test(file_exclude_quoted_${lang} ${lang} ${_sif_src_${lang}}
    tests/sif/file_exclude_quoted.tau WARNINGS "${_warning}")
  set_tests_properties(check_sif_file_exclude_quoted_${lang}
    PROPERTIES PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}")
  string(REPLACE "%s" "include" _warning "${_sif_quoted_file_warning}")
  add_sif_test(file_include_quoted_${lang} ${lang} ${_sif_src_${lang}}
    tests/sif/file_include_quoted.tau WARNINGS "${_warning}")
  _sif_check_file_excluded(file_include_quoted_${lang})
endforeach()

# An inline function on the include list gets a timer without
# --tau_instrument_inline; a routine off the list gets none.
add_sif_test(include_inline_cpp cpp tests/instrument_inline.cpp tests/sif/include_inline_cpp.tau)
set_tests_properties(check_sif_include_inline_cpp
  PROPERTIES
  PASS_REGULAR_EXPRESSION "TAU_PROFILE_TIMER[^\n]*free_fn_marked_inline"
  FAIL_REGULAR_EXPRESSION
    "TAU_PROFILE_TIMER[^\n]*method_in_class_body;TAU_PROFILE_TIMER[^\n]*int main"
)

# saltfm gives cparse-llvm and fparse-llvm the canonical level of
# --tau_main_setup_parse_level, or else of SALT_MAIN_SETUP_PARSE_LEVEL:
# never, function, or always. Values ignore case, an empty one selects
# `function`, and an unknown one stops saltfm with the usage (#88).
# Adds test saltfm_level_<name>: `saltfm --show ARGN hello.c`, with
# SALT_MAIN_SETUP_PARSE_LEVEL set to `value` (unset for `unset`), must
# match `pass` and must not match `fail`.
function(_salt_add_level_test name value pass fail)
  set(mod "SALT_MAIN_SETUP_PARSE_LEVEL=set:${value}")
  if(value STREQUAL "unset")
    set(mod "SALT_MAIN_SETUP_PARSE_LEVEL=unset:")
  endif()
  add_test(NAME saltfm_level_${name}
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show ${ARGN}
            ${CMAKE_SOURCE_DIR}/tests/hello.c)
  set_tests_properties(saltfm_level_${name} PROPERTIES
    PASS_REGULAR_EXPRESSION "${pass}"
    FAIL_REGULAR_EXPRESSION "${fail}"
    ENVIRONMENT_MODIFICATION "${mod}"
    LABELS "lang:C;phase:smoke")
endfunction()
set(_level_shown "cparse-llvm --tau_main_setup_parse_level=")
# Each value, in capitals, with another level in the environment, which
# the option overrides.
foreach(pair
    never:never no:never off:never false:never 0:never
    function:function procedure:function default:function medium:function
    yes:function on:function true:function 1:function
    always:always file:always force:always all:always)
  string(REPLACE ":" ";" _fields "${pair}")
  list(GET _fields 0 _value)
  list(GET _fields 1 _level)
  set(_other always)
  if(_level STREQUAL "always")
    set(_other never)
  endif()
  string(TOUPPER ${_value} _upper)
  _salt_add_level_test(value_${_value} ${_other} "${_level_shown}${_level} " ""
    --tau_main_setup_parse_level=${_upper})
endforeach()
_salt_add_level_test(env Always "${_level_shown}always " "")
_salt_add_level_test(env_empty "" "${_level_shown}function " "")
_salt_add_level_test(default unset "${_level_shown}function " "")
_salt_add_level_test(option_empty never "${_level_shown}function " ""
  --tau_main_setup_parse_level=)
# The value can also be the next argument. Without one, the option at the
# end gives a warning and is left out.
_salt_add_level_test(option_next never "${_level_shown}always " ""
  --tau_main_setup_parse_level Always)
string(CONCAT _level_no_value
  "WARNING: --tau_main_setup_parse_level needs a value. using\n"
  "  SALT_MAIN_SETUP_PARSE_LEVEL=always\n.*${_level_shown}always [^\n]*/hello[.]c\n")
add_test(NAME saltfm_level_no_value
  COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
          ${CMAKE_SOURCE_DIR}/tests/hello.c --tau_main_setup_parse_level)
set_tests_properties(saltfm_level_no_value PROPERTIES
  PASS_REGULAR_EXPRESSION "${_level_no_value}"
  ENVIRONMENT_MODIFICATION "SALT_MAIN_SETUP_PARSE_LEVEL=set:always"
  LABELS "lang:C;phase:smoke")
_salt_add_level_test(unknown never
  "ERROR: unknown value 'sometimes' for --tau_main_setup_parse_level\n.*USAGE:"
  "${_level_shown}" --tau_main_setup_parse_level=sometimes)
_salt_add_level_test(unknown_env sometimes
  "ERROR: unknown value 'sometimes' for SALT_MAIN_SETUP_PARSE_LEVEL\n.*USAGE:"
  "${_level_shown}")
# cparse-llvm takes only the canonical names.
add_test(NAME cparse_level_canonical_only
  COMMAND $<TARGET_FILE:cparse-llvm> --tau_main_setup_parse_level=yes
          ${CMAKE_SOURCE_DIR}/tests/hello.c --)
set_tests_properties(cparse_level_canonical_only PROPERTIES
  PASS_REGULAR_EXPRESSION "Cannot find option named 'yes'"
  LABELS "lang:C;phase:smoke")
if(TEST_FORTRAN)
  # saltfm gives fparse-llvm the canonical level, which fparse-llvm gives
  # the plugin; fparse-llvm takes only the canonical names.
  add_test(NAME saltfm_level_fortran
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            --tau_main_setup_parse_level=Off
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  add_test(NAME fparse_level_canonical_only
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_main_setup_parse_level=off
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(saltfm_level_fortran PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_MAIN_SETUP_PARSE_LEVEL=never\ncmd: ")
  set_tests_properties(fparse_level_canonical_only PROPERTIES
    PASS_REGULAR_EXPRESSION "ERROR: unknown value 'off' for --tau_main_setup_parse_level"
    FAIL_REGULAR_EXPRESSION "cmd: ")
  # Both also take the value as the next argument.
  add_test(NAME saltfm_level_fortran_next
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            --tau_main_setup_parse_level never
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  add_test(NAME fparse_level_next
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_main_setup_parse_level always
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(saltfm_level_fortran_next PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_MAIN_SETUP_PARSE_LEVEL=never\ncmd: ")
  set_tests_properties(fparse_level_next PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_MAIN_SETUP_PARSE_LEVEL=always\ncmd: ")
  add_test(NAME fparse_level_no_value
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 --tau_main_setup_parse_level)
  string(CONCAT _level_no_value
    "WARNING: --tau_main_setup_parse_level needs a value. using function\n"
    ".*\nSALT_FORTRAN_MAIN_SETUP_PARSE_LEVEL=function\ncmd: ")
  set_tests_properties(fparse_level_no_value PROPERTIES
    PASS_REGULAR_EXPRESSION "${_level_no_value}")
  set_tests_properties(saltfm_level_fortran fparse_level_canonical_only
    saltfm_level_fortran_next fparse_level_next fparse_level_no_value PROPERTIES
    ENVIRONMENT_MODIFICATION "SALT_MAIN_SETUP_PARSE_LEVEL=unset:"
    LABELS "lang:Fortran;phase:smoke")
endif()

# At `never`, a main program that the routine lists leave out gets no
# setup code, and saltfm does not warn about the include list; at
# `always`, it gets the setup code, as at `function`.
foreach(lang IN LISTS _sif_langs)
  foreach(level never always)
    add_sif_test(main_excluded_${level}_${lang} ${lang} ${_sif_src_${lang}}
      tests/sif/exclude_main.tau ARGS --tau_main_setup_parse_level=${level})
    set_tests_properties(check_sif_main_excluded_${level}_${lang}
      PROPERTIES FAIL_REGULAR_EXPRESSION "${_sif_main_timer_${lang}}")
  endforeach()
  set_property(TEST check_sif_main_excluded_never_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  set_tests_properties(check_sif_main_excluded_always_${lang}
    PROPERTIES PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  add_sif_test(include_list_never_${lang} ${lang} ${_sif_src_${lang}}
    tests/sif/include_list.tau ARGS --tau_main_setup_parse_level=never)
  set_tests_properties(check_sif_include_list_never_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
    FAIL_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  set_property(TEST warn_sif_include_list_never_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "${_sif_main_warning}")
endforeach()

# TAU's selective instrumentation reference cases (tests/sif/tau/):
# sif_check reads each case's SIF with SALT-FM's parser and checks the
# routine and file names of its .expect file with SALT-FM's matcher.
add_executable(sif_check tests/sif_check.cpp src/selectfile.cpp)
target_include_directories(sif_check PRIVATE
  ${CMAKE_SOURCE_DIR}/include ${LLVM_INCLUDE_DIRS})
target_compile_definitions(sif_check PRIVATE ${LLVM_DEFINITIONS_LIST})
foreach(case
    01_comments 02_exclude_wildcard 03_include 04_exclude_beats_include
    05_quoted 06_metachars 07_files 08_file_exclude_only 09_stray_lines
    10_instrument_section 11_empty 12_mixed 14_whitespace
    15_slash_in_function)
  add_test(NAME sif_tau_${case}
    COMMAND sif_check ${CMAKE_SOURCE_DIR}/tests/sif/tau/${case}.tau
            ${CMAKE_SOURCE_DIR}/tests/sif/tau/${case}.expect)
  set_tests_properties(sif_tau_${case} PROPERTIES LABELS "phase:check;sif")
endforeach()

# The README's file pattern table: each pattern alone on a file include
# list, against paths as given on the compile line. Each row is the
# pattern and whether it matches src/a.c, ./src/a.c, /abs/src/a.c and
# mysrc/a.c.
set(_sif_path_paths src/a.c ./src/a.c /abs/src/a.c mysrc/a.c)
set(_sif_path_rows
  "src/a.c:yes:no:no:no"
  "*/src/a.c:no:yes:yes:no"
  "*src/a.c:yes:yes:yes:yes"
  "a.c:yes:yes:yes:yes")
set(_row 0)
foreach(row IN LISTS _sif_path_rows)
  string(REPLACE ":" ";" row "${row}")
  list(POP_FRONT row pattern)
  set(_expect "")
  foreach(path want IN ZIP_LISTS _sif_path_paths row)
    string(APPEND _expect "file: ${path} => ${want}\n")
  endforeach()
  math(EXPR _row "${_row} + 1")
  set(_case ${CMAKE_BINARY_DIR}/sif/path_table_${_row})
  file(WRITE ${_case}.tau "BEGIN_FILE_INCLUDE_LIST\n${pattern}\nEND_FILE_INCLUDE_LIST\n")
  file(WRITE ${_case}.expect "${_expect}")
  add_test(NAME sif_path_table_${_row} COMMAND sif_check ${_case}.tau ${_case}.expect)
  set_tests_properties(sif_path_table_${_row} PROPERTIES LABELS "phase:check;sif")
endforeach()

# A line longer than the old 64 KiB line buffer must not end the parse
# and drop the lists after it.
string(REPEAT "x" 70000 _sif_long_comment)
file(WRITE ${CMAKE_BINARY_DIR}/sif/long_line.tau
  "# ${_sif_long_comment}\nBEGIN_EXCLUDE_LIST\n\"#excluded_routine#\"\nEND_EXCLUDE_LIST\n")
add_sif_test(long_line_c c tests/sif_excl.c ${CMAKE_BINARY_DIR}/sif/long_line.tau)
set_tests_properties(check_sif_long_line_c
  PROPERTIES
  PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_c}"
  FAIL_REGULAR_EXPRESSION "${_sif_excluded_pattern_c}"
)

# The BEGIN_INSTRUMENT_SECTION warning goes to stderr, on its own line.
add_test(NAME sif_instrument_section_warning
  COMMAND sh -c "\"$0\" \"$@\" 2>&1 >/dev/null"
          "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm"
          --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/tau/10_instrument_section.tau
          --tau_output=sif_instrument_section.inst.c
          ${CMAKE_SOURCE_DIR}/tests/sif_excl.c
)
set_tests_properties(sif_instrument_section_warning
  PROPERTIES
  LABELS "lang:C;phase:instrument;sif"
  PASS_REGULAR_EXPRESSION "WARNING: Instrument section is not supported yet[.]\n"
)
_salt_in_test_output(sif sif_instrument_section_warning)

# Fabrication tests (--tau_fabricate_unknown_types):
function(add_fabricate_test name src with_flag pass_regex)
  set(fail_regex "")
  set(summary_regex
      "Fabricated [0-9]+ namespaces, [0-9]+ class templates")
  if(ARGC GREATER 4)
    set(fail_regex "${ARGV4}")
  endif()
  if(ARGC GREATER 5)
    set(summary_regex "${ARGV5}")
  endif()
  get_filename_component(_src_ext ${src} EXT)
  set(out_file ${name}.fab.inst${_src_ext})
  _salt_ext_to_label(${src} _lang_label)
  if(with_flag)
    set(flag_args --tau_fabricate_unknown_types)
    set(instrument_pass_pattern "${summary_regex}")
  else()
    set(flag_args "")
    set(instrument_pass_pattern "[Ii]nstrumentation:")
  endif()
  add_test(NAME instrument_fabricate_${name}
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
            ${flag_args}
            --tau_output=${out_file}
            ${CMAKE_SOURCE_DIR}/tests/${src}
  )
  set_tests_properties(instrument_fabricate_${name}
    PROPERTIES
    REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/${src}"
    ENVIRONMENT "SALT_FABRICATE_UNKNOWN_TYPES=0"
    FIXTURES_SETUP instrument_fabricate_${name}
    LABELS "${_lang_label};phase:instrument;fabricate"
    PASS_REGULAR_EXPRESSION "${instrument_pass_pattern}"
  )
  add_test(NAME check_fabricate_${name}
    COMMAND ${CMAKE_COMMAND} -E cat ./${out_file}
  )
  set_tests_properties(check_fabricate_${name}
    PROPERTIES
    FIXTURES_REQUIRED instrument_fabricate_${name}
    ENVIRONMENT "SALT_FABRICATE_UNKNOWN_TYPES=0"
    LABELS "${_lang_label};phase:check;fabricate"
    PASS_REGULAR_EXPRESSION "${pass_regex}"
  )
  if(NOT fail_regex STREQUAL "")
    set_tests_properties(check_fabricate_${name}
      PROPERTIES
      FAIL_REGULAR_EXPRESSION "${fail_regex}"
    )
  endif()
  _salt_in_test_output(fabricate
    instrument_fabricate_${name} check_fabricate_${name})
endfunction()

add_fabricate_test(case1 fabricate_case1.cpp ON
  "int test[(]testns::lemur<int>[)]")
add_fabricate_test(nested_ns fabricate_nested_ns.cpp ON
  "test2[(]aa::bb::cc<int>[)]")
add_fabricate_test(return_type fabricate_return_type.cpp ON
  "qq::rr<float> test3[(]int[)]")
add_fabricate_test(multi_param fabricate_multi_param.cpp ON
  "test4[(]n1::t1<int>, n2::t2<char>[)]")
add_fabricate_test(case2 fabricate_case2.cpp ON
  "int test[(]lemur<int>[)]" ""
  "Fabricated 0 namespaces, 0 class templates")
add_fabricate_test(case3 fabricate_case3.cpp ON
  "int test[(]float, testns::lemur<int>[)]")
add_fabricate_test(reuse fabricate_reuse.cpp ON
  "int first[(]testns::lemur<int>[)]" ""
  "Fabricated 1 namespaces, 1 class templates")

add_test(NAME check_fabricate_reuse_second
  COMMAND ${CMAKE_COMMAND} -E cat ./reuse.fab.inst.cpp
)
set_tests_properties(check_fabricate_reuse_second
  PROPERTIES
  FIXTURES_REQUIRED instrument_fabricate_reuse
  LABELS "lang:CXX;phase:check;fabricate"
  PASS_REGULAR_EXPRESSION "int second[(]testns::lemur<int>[)]"
)

add_fabricate_test(plain_qualified fabricate_plain_qualified.cpp ON
  "int test[(]testns::Foo[)]" ""
  "Fabricated 1 namespaces, 0 class templates, 1 records")

add_fabricate_test(mixed_kinds fabricate_mixed_kinds.cpp ON
  "int usesPlain[(]testns::Foo[)]" ""
  "Fabricated 1 namespaces, 1 class templates, 1 records")

add_test(NAME check_fabricate_mixed_kinds_template
  COMMAND ${CMAKE_COMMAND} -E cat ./mixed_kinds.fab.inst.cpp
)
set_tests_properties(check_fabricate_mixed_kinds_template
  PROPERTIES
  FIXTURES_REQUIRED instrument_fabricate_mixed_kinds
  LABELS "lang:CXX;phase:check;fabricate"
  PASS_REGULAR_EXPRESSION "int usesTemplate[(]testns::lemur<int>[)]"
)

add_fabricate_test(nontype_args fabricate_nontype_args.cpp ON
  "int test[(]testns::arr<int, 3>[)]")
add_fabricate_test(member_def fabricate_member_def.cpp ON
  "void MyClass::method[(]void[)]")

add_test(NAME check_fabricate_member_def_ctor
  COMMAND ${CMAKE_COMMAND} -E cat ./member_def.fab.inst.cpp
)
set_tests_properties(check_fabricate_member_def_ctor
  PROPERTIES
  FIXTURES_REQUIRED instrument_fabricate_member_def
  LABELS "lang:CXX;phase:check;fabricate"
  PASS_REGULAR_EXPRESSION "int MyClass::MyClass[(]void[)]"
)

add_fabricate_test(class_member fabricate_class_member.cpp ON
  "int f[(]Real::Unknown[)]" ""
  "Fabricated 0 namespaces, 0 class templates")
add_fabricate_test(control_on fabricate_control.cpp ON
  "int add[(]int, int[)]" ""
  "Fabricated 0 namespaces, 0 class templates")
add_fabricate_test(control_off fabricate_control.cpp OFF
  "int add[(]int, int[)]")
add_fabricate_test(c_control fabricate_control.c ON
  "int add[(]int, int[)]" ""
  "Fabricated 0 namespaces, 0 class templates")
# Canary: without the flag, fabricate_case1.cpp does not compile and
# saltfm fails, so the fabricate tests above prove the flag matters.
_salt_add_clang_error_test(fabricate_canary canary.fab.inst.cpp
  "lang:CXX;fabricate" fabricate_case1.cpp)
# With the flag, the same Clang errors are expected: exit 0, the
# warning, and output.
_salt_add_clang_error_test(fabricate_on_exit_zero fabricate_on.inst.cpp
  "lang:CXX;fabricate" fabricate_case1.cpp EXIT_ZERO
  CHECK "-s fabricate_on.inst.cpp" ARGS --tau_fabricate_unknown_types
  LOG "${_allow_warning}")

add_test(NAME instrument_fabricate_env
  COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
          --tau_output=env.fab.inst.cpp
          ${CMAKE_SOURCE_DIR}/tests/fabricate_case1.cpp
)
set_tests_properties(instrument_fabricate_env
  PROPERTIES
  REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/fabricate_case1.cpp"
  ENVIRONMENT "SALT_FABRICATE_UNKNOWN_TYPES=1"
  FIXTURES_SETUP instrument_fabricate_env
  LABELS "lang:CXX;phase:instrument;fabricate"
  PASS_REGULAR_EXPRESSION
    "Fabricated [0-9]+ namespaces, [0-9]+ class templates"
)
add_test(NAME check_fabricate_env
  COMMAND ${CMAKE_COMMAND} -E cat ./env.fab.inst.cpp
)
set_tests_properties(check_fabricate_env
  PROPERTIES
  FIXTURES_REQUIRED instrument_fabricate_env
  LABELS "lang:CXX;phase:check;fabricate"
  PASS_REGULAR_EXPRESSION "int test[(]testns::lemur<int>[)]"
)
_salt_in_test_output(fabricate
  check_fabricate_reuse_second check_fabricate_mixed_kinds_template
  check_fabricate_member_def_ctor instrument_fabricate_env check_fabricate_env)

add_fabricate_test(srctext_zmobius fabricate_srctext_zmobius.cpp ON
  "void computeZmobiusOmega[(]std::vector<ComplexD> &, const int, \
const std::vector<RealD> &, const int, const RealD[)]")
add_fabricate_test(srctext_default_arg fabricate_srctext_default_arg.cpp ON
  "void withDefault[(]testns::tmpl<int>[)]"
  "TAU_PROFILE_TIMER[^\n]*tmpl<int> *=")
add_fabricate_test(srctext_fallback fabricate_srctext_fallback.cpp ON
  "void fallbackUnnamed[(]int[)]")
add_fabricate_test(srctext_comment fabricate_srctext_comment.cpp ON
  "void commented[(]std::vector<RealD> &[)]"
  "TAU_PROFILE_TIMER[^\n]*holds")
add_fabricate_test(srctext_multiline fabricate_srctext_multiline.cpp ON
  "void multiline[(]std::vector< RealD> &, const int[)]")
add_fabricate_test(srctext_fn_template fabricate_srctext_fn_template.cpp ON
  "void tmplFn[(]std::vector<RealD> &, T[)]")

set(SALT_COMPILERS_TO_TEST gcc clang)
# Per-toolchain build subdirs (GCC, LLVM) of test-output/tau with one
# subdir per language: TAU names objects and profiles after the source
# basename, so hello.c and hello.f90 must not build in the same directory.
set(SALT_TAU_TEST_DIR ${SALT_TEST_OUTPUT_DIR}/tau)
foreach(tc_name IN ITEMS GCC LLVM)
  add_test(NAME setup_${tc_name}_dir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SALT_TAU_TEST_DIR}/${tc_name}/c
      ${SALT_TAU_TEST_DIR}/${tc_name}/cxx ${SALT_TAU_TEST_DIR}/${tc_name}/fortran)
  set_tests_properties(setup_${tc_name}_dir
    PROPERTIES
    FIXTURES_SETUP ${tc_name}_dir
    LABELS "toolchain:${tc_name};phase:setup"
  )
endforeach()

if(HAVE_TAU)
  # Resolve test source language to TAU wrapper + on-disk path. Sets
  # ${out_tauc_var} and ${out_path_var} in the caller's scope.
  function(_salt_tau_resolve_lang test_base_name test_lang
      out_tauc_var out_path_var)
    if(${test_lang} MATCHES "^[cC]$")
      set(tauc_local ${TAUCC})
      set(path_local ${CMAKE_SOURCE_DIR}/tests/${test_base_name}.${test_lang})
    elseif(${test_lang} MATCHES "^(cpp|CPP)$")
      set(tauc_local ${TAUCXX})
      set(path_local ${CMAKE_SOURCE_DIR}/tests/${test_base_name}.${test_lang})
    elseif(${test_lang} MATCHES "^[fF](90)?$")
      set(tauc_local ${TAUF90})
      set(path_local
        ${CMAKE_SOURCE_DIR}/tests/fortran/${test_base_name}.${test_lang})
    else()
      message(FATAL_ERROR "Unknown test source file extension: ${test_lang}")
    endif()
    set(${out_tauc_var} ${tauc_local} PARENT_SCOPE)
    set(${out_path_var} ${path_local} PARENT_SCOPE)
  endfunction()

  # Emit profile-dir cleanup + creation fixture pair. `comp_id`/`test_id` go
  # into test names; `test_base` names the .d profile dir; `dir` is the
  # toolchain and language subdir of test-output/tau (e.g. GCC/c);
  # `fixture_name` is the FIXTURES_SETUP value; `dir_fix` is the fixture
  # that creates `dir`.
  function(_salt_tau_emit_profile_dir_fixture
      comp_id test_id test_base dir
      lang_label tc_label fixture_name dir_fix)
    add_test(NAME rm_old_${comp_id}_${test_id}_profiles
      COMMAND ${CMAKE_COMMAND} -E rm -rf ${test_base}.d
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${dir}
    )
    set_tests_properties(rm_old_${comp_id}_${test_id}_profiles
      PROPERTIES
      FIXTURES_SETUP ${fixture_name}
      FIXTURES_REQUIRED ${dir_fix}
      LABELS "${lang_label};${tc_label};phase:setup"
    )
    add_test(NAME mkdir_${test_base}.${comp_id}.d
      COMMAND ${CMAKE_COMMAND} -E make_directory ${test_base}.d
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${dir}
    )
    set_tests_properties(mkdir_${test_base}.${comp_id}.d
      PROPERTIES
      FIXTURES_SETUP ${fixture_name}
      FIXTURES_REQUIRED ${dir_fix}
      DEPENDS rm_old_${comp_id}_${test_id}_profiles
      LABELS "${lang_label};${tc_label};phase:setup"
    )
  endfunction()

  # Emit run + check_*_profile pair. Keyword args:
  #   COMP_ID/TEST_ID  — go into emitted test names.
  #   TEST_BASE        — names the .d profile dir.
  #   DIR              — toolchain and language subdir of test-output/tau
  #                      (e.g. GCC/c).
  #   BINARY           — executable basename in ${SALT_TAU_TEST_DIR}/${DIR}.
  #   LANG_LABEL/TC_LABEL — ctest LABELS.
  #   TAU_MAKEFILE     — resolved TAU Makefile path.
  #   COMPILE_DEP      — compile test, and its FIXTURES_SETUP name, that
  #                      run_* needs, so it never uses a stale binary
  #                      after a failed compile. run_* is in turn the
  #                      fixture check_*_profile needs.
  #   PROFILE_FIX      — FIXTURES_REQUIRED set name (profile-dir fixture).
  function(_salt_tau_emit_run_check)
    set(kw COMP_ID TEST_ID TEST_BASE DIR BINARY LANG_LABEL TC_LABEL
        TAU_MAKEFILE COMPILE_DEP PROFILE_FIX)
    cmake_parse_arguments(ARG "" "${kw}" "" ${ARGN})
    _salt_tau_exec_tags(_run_tau_tags "${ARG_TAU_MAKEFILE}")
    add_test(NAME run_${ARG_COMP_ID}_${ARG_TEST_ID}
      COMMAND ${TAU_EXEC} -T ${_run_tau_tags} ./${ARG_BINARY}
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${ARG_DIR}
    )
    set_tests_properties(run_${ARG_COMP_ID}_${ARG_TEST_ID}
      PROPERTIES
      ENVIRONMENT
        "TAU_MAKEFILE=${ARG_TAU_MAKEFILE};PROFILEDIR=${ARG_TEST_BASE}.d"
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${TAU_BIN_DIR}"
      FIXTURES_REQUIRED "${ARG_PROFILE_FIX};${ARG_COMPILE_DEP}"
      FIXTURES_SETUP run_${ARG_COMP_ID}_${ARG_TEST_ID}
      LABELS "${ARG_LANG_LABEL};${ARG_TC_LABEL};phase:run"
      FAIL_REGULAR_EXPRESSION
        "[Cc]ommand not found;[Ss]egmentation;[Ff]ault;[Ee]rror"
    )
    add_test(NAME check_${ARG_COMP_ID}_${ARG_TEST_ID}_profile
      COMMAND ${CMAKE_COMMAND} -E cat ./${ARG_TEST_BASE}.d/profile.0.0.0
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${ARG_DIR}
    )
    set_tests_properties(check_${ARG_COMP_ID}_${ARG_TEST_ID}_profile
      PROPERTIES
      PASS_REGULAR_EXPRESSION "GROUP=\"TAU_DEFAULT\""
      FAIL_REGULAR_EXPRESSION "addr=\<0x"
      FIXTURES_REQUIRED run_${ARG_COMP_ID}_${ARG_TEST_ID}
      LABELS "${ARG_LANG_LABEL};${ARG_TC_LABEL};phase:check"
    )
  endfunction()

  # Emit per-compiler TAU build/run/profile tests for one C/C++ test source.
  # Reads tauc/tauc_opts/compile_opts/compile_fail_patterns/_lang_label/
  # test_name/test_base_name/test_lang/test_path/
  # _${test_base_name}_extra_sources from compile_instrumented's scope.
  function(_salt_tau_emit_compiler_tests comp)
    set(lower_comp ${comp})
    _salt_compiler_to_toolchain(${comp} tc_label)
    _salt_compiler_to_dir(${comp} dir)
    # Language subdir of the toolchain dir: c or cxx
    string(TOLOWER "${_lang_label}" lang_dir)
    string(REPLACE "lang:" "${dir}/" lang_dir "${lang_dir}")
    set(obj_fix clean_${lower_comp}_${test_name}_objects)
    set(prof_fix clean_${lower_comp}_${test_name}_profiles)
    add_test(NAME rm_${lower_comp}_${test_name}_objects
      COMMAND ${CMAKE_COMMAND} -E rm -rf
        ${test_base_name}.o
        ${test_base_name}.inst.o
        ${test_base_name}.inst.${test_lang}
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${lang_dir}
    )
    set_tests_properties(rm_${lower_comp}_${test_name}_objects
      PROPERTIES
      FIXTURES_SETUP ${obj_fix}
      FIXTURES_REQUIRED ${dir}_dir
      LABELS "${_lang_label};${tc_label};phase:setup"
    )
    # Per-test extra sources can be supplied via an
    # `_${test_base_name}_extra_sources` variable in the caller's scope
    # (e.g. forward_decl ships a stub TU that defines a symbol the fixture
    # deliberately leaves bodyless to exercise #53 regression coverage).
    add_test(NAME compile_${lower_comp}_${test_name}
      COMMAND ${tauc} ${tauc_opts} ${compile_opts}
        -o ${test_base_name}.${lower_comp}
        ${test_path} ${_${test_base_name}_extra_sources}
      WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${lang_dir}
    )
    set_tests_properties(compile_${lower_comp}_${test_name}
      PROPERTIES
      ENVIRONMENT TAU_MAKEFILE=${TAU_${dir}_MAKEFILE}
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${TAU_BIN_DIR}"
      FIXTURES_REQUIRED ${obj_fix}
      FIXTURES_SETUP compile_${lower_comp}_${test_name}
      LABELS "${_lang_label};${tc_label};phase:compile"
      FAIL_REGULAR_EXPRESSION "${compile_fail_patterns}"
    )
    _salt_tau_emit_profile_dir_fixture(
      ${lower_comp} ${test_name} ${test_base_name} ${lang_dir}
      "${_lang_label}" "${tc_label}" ${prof_fix} ${dir}_dir)
    _salt_tau_emit_run_check(
      COMP_ID ${lower_comp}
      TEST_ID ${test_name}
      TEST_BASE ${test_base_name}
      DIR ${lang_dir}
      BINARY ${test_base_name}.${lower_comp}
      LANG_LABEL "${_lang_label}"
      TC_LABEL "${tc_label}"
      TAU_MAKEFILE "${TAU_${dir}_MAKEFILE}"
      COMPILE_DEP compile_${lower_comp}_${test_name}
      PROFILE_FIX ${prof_fix})
  endfunction()

  # Add a TAU build/run/profile pipeline for a single test source. Optional
  # second argument overrides the test name (must match the prior
  # add_instrumentor_test invocation). Using the optional argument here and
  # in add_instrumentor_test is untested and may cause issues; a more robust
  # design might be to incorporate both functions into a single function.
  function(compile_instrumented test_src)
    get_filename_component(test_base_name ${test_src} NAME_WE)
    set(test_name "${test_base_name}")
    get_filename_component(test_lang ${test_src} LAST_EXT)
    string(REPLACE "." "" test_lang ${test_lang})
    set(extra_args ${ARGN})
    list(LENGTH extra_args n_extra_args)
    if(n_extra_args GREATER 1)
      message(AUTHOR_WARNING
        "Incorrect number of arguments (1 + ${n_extra_args}) "
        "passed to add_instrumentor_test!")
      return()
    endif()
    if(n_extra_args EQUAL 1)
      set(test_name ${ARGV1})
    endif()
    _salt_tau_resolve_lang(
      ${test_base_name} ${test_lang} tauc test_path)
    set(tauc_opts
      -optVerbose -optNoRevert -optSaltInst
      -optSaltParser=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm)
    if(APPLE)
      list(APPEND tauc_opts -optShared)
    endif()
    set(compile_opts ${TAU_COMPILE_OPTIONS})
    # Each entry is one regex; CMake joins with `;` to form the
    # FAIL_REGULAR_EXPRESSION list (any matching pattern fails the test).
    set(compile_fail_patterns
      "[Dd]isabling instrumentation of source code"
      "[Ss]witching to compiler-based instrumentation"
      "[Cc]ompiling with [Nn]on-[Ii]nstrumented [Rr]egular [Cc]ode"
      "[Ee]rror:")
    _salt_ext_to_label(${test_src} _lang_label)
    foreach(comp IN LISTS SALT_COMPILERS_TO_TEST)
      _salt_tau_emit_compiler_tests(${comp})
    endforeach()
  endfunction()

  # This works, but a more thorough comparison of the fortran test setup
  # with the C/C++ setup is needed and the directory naming is awkward when
  # using Fortran sources in the above function.
  # list(APPEND TESTS_LIST
  #   funcsub.f90
  #   myhi.f
  # )

  # Pair forward_decl.c with a stub TU that defines the symbol the
  # fixture deliberately leaves bodyless.  cparse-llvm still only
  # parses forward_decl.c (so #53's bodyless-FunctionDecl path is
  # exercised); the stub satisfies the link so we can also assert the
  # instrumented binary runs and produces a TAU profile.
  set(_forward_decl_extra_sources ${CMAKE_SOURCE_DIR}/tests/forward_decl_stub.c)

  foreach(test_source IN LISTS TESTS_LIST)
    compile_instrumented(${test_source})
  endforeach()
endif()

if(TEST_FORTRAN)
  # Add some Fortran tests for SALT-FM
  set(FORTRAN_TESTS_SOURCES_LIST
    myhi.f
    cubes.f
    emptyprog.f90
    funcsub.f90
    hello.f90
    loop_test.f90
    trivial.f90
    return-only.f90
    if-stmt.f90
    if-stmt-edge.f90
    if-stmt-continuation.f90
    if-stmt-labeled.f90
    implicit-main.f90
    alternate-return.f90
    label-do-at-end.f90
    internal-func.f90
    interface_test.f90
    submodule_test.f90
    separate-module-procedure.f90
    pure-and-elemental.f90
    use-intrinsic.f90
    use-intrinsic-c-binding.f90
  )

  # Sources that use obsolescent (but still standard) features
  # require relaxed compile flags: -Wpedantic emits warnings the
  # other tests treat as errors via -Werror.  Drop both for these
  # entries so the pipeline can compile, run, and profile them.
  set(FORTRAN_TESTS_PERMISSIVE_SOURCES
    alternate-return.f90
    label-do-at-end.f90
    separate-module-procedure.f90
  )

  # Add a smoke test of the fparse-llvm script
  add_test(NAME fparse_llvm_smoke_test
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --help)
  set_tests_properties(fparse_llvm_smoke_test
    PROPERTIES
    LABELS "lang:Fortran;phase:smoke"
    PASS_REGULAR_EXPRESSION "USAGE"
  )

  # Flang driver selection in fparse-llvm. Stub drivers in
  # flang_exe/<kind>/{flang,flang-new}: `fail` fails whenever it runs,
  # `match` and `other` report the build's LLVM major version and
  # another one, then fail for anything but --version. `nodriver` holds
  # only what the wrappers need to start: bash, but not `dirname`. fparse-llvm-no-driver and
  # fparse-llvm-no-record are copies of fparse-llvm whose recorded driver
  # is missing, or was not found at configure time.
  set(_fx_dir ${CMAKE_BINARY_DIR}/flang_exe)
  set(_fx_bin ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set(_fx_src ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  math(EXPR _fx_other "${LLVM_VERSION_MAJOR} + 1")
  foreach(fx_name flang flang-new)
    configure_file(${CMAKE_SOURCE_DIR}/tests/fortran/flang_exe/fail
      ${_fx_dir}/fail/${fx_name} COPYONLY)
    set(_fx_major ${LLVM_VERSION_MAJOR})
    configure_file(${CMAKE_SOURCE_DIR}/tests/fortran/flang_exe/flang.in
      ${_fx_dir}/match/${fx_name} @ONLY)
    set(_fx_major ${_fx_other})
    configure_file(${CMAKE_SOURCE_DIR}/tests/fortran/flang_exe/flang.in
      ${_fx_dir}/other/${fx_name} @ONLY)
  endforeach()
  # A fresh directory, so no tool left by an older build stays in it
  file(REMOVE_RECURSE ${_fx_dir}/nodriver)
  file(MAKE_DIRECTORY ${_fx_dir}/nodriver)
  find_program(_fx_tool bash REQUIRED NO_CACHE)
  file(CREATE_LINK ${_fx_tool} ${_fx_dir}/nodriver/bash SYMBOLIC)
  unset(_fx_tool)
  _salt_configure_fparse_llvm(${_fx_bin}/fparse-llvm-no-driver
    ${_fx_dir}/missing/flang)
  _salt_configure_fparse_llvm(${_fx_bin}/fparse-llvm-no-record
    _salt_flang_driver-NOTFOUND)

  # Add test `name` running ARGN whose output must match `pass` and
  # must not match `fail`, with SALT_FLANG_EXE unset.
  function(_salt_add_flang_exe_test name pass fail)
    add_test(NAME ${name} COMMAND ${ARGN})
    set_tests_properties(${name} PROPERTIES
      PASS_REGULAR_EXPRESSION "${pass}"
      FAIL_REGULAR_EXPRESSION "${fail}"
      ENVIRONMENT_MODIFICATION "SALT_FLANG_EXE=unset:"
      LABELS "lang:Fortran;phase:smoke")
    _salt_in_test_output(flang_exe ${name})
  endfunction()
  set(_fx_stub_ran "stub driver must not run")
  set(_fx_note "Note: [^\n]*flang-new[^\n]* flang")
  set(_fx_match_cmd "cmd: [^ ]*/flang_exe/match/flang -fc1 ")

  # Stubs earlier on PATH do not replace the recorded driver.
  _salt_add_flang_exe_test(fparse_flang_exe_recorded
    "SALT Instrumentor Plugin finished" "${_fx_stub_ran};WARNING: recorded;${_fx_note}"
    ${_fx_bin}/fparse-llvm --tau_output=recorded.inst.F90
    ${_fx_src})
  set_tests_properties(fparse_flang_exe_recorded PROPERTIES
    ENVIRONMENT "SALT_FORTRAN_VERBOSE=1")
  set_property(TEST fparse_flang_exe_recorded APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_fx_dir}/fail")

  # A missing recorded driver warns and falls back to PATH.
  string(CONCAT _fx_pass
    "WARNING: [^\n]*'[^']*/flang_exe/missing/flang'.*"
    "Running: /[^ ]*/flang(-new)? -fc1 .*"
    "SALT Instrumentor Plugin finished")
  _salt_add_flang_exe_test(fparse_flang_exe_missing_recorded
    "${_fx_pass}" "ERROR"
    ${_fx_bin}/fparse-llvm-no-driver --tau_output=path.inst.F90
    ${_fx_src})
  set_tests_properties(fparse_flang_exe_missing_recorded PROPERTIES
    ENVIRONMENT "SALT_FORTRAN_VERBOSE=1")

  # A driver of another LLVM major version stops before it runs.
  string(CONCAT _fx_pass
    "ERROR: Flang driver '[^']*/flang_exe/other/flang' has LLVM major version "
    "${_fx_other},\nbut SALT-FM was built with LLVM ${LLVM_VERSION_MAJOR}\\..*"
    "--flang_exe=<path> or SALT_FLANG_EXE")
  _salt_add_flang_exe_test(fparse_flang_exe_other_version
    "${_fx_pass}" "${_fx_stub_ran};Running:"
    ${_fx_bin}/fparse-llvm-no-driver --tau_output=other.inst.F90
    ${_fx_src})
  set_property(TEST fparse_flang_exe_other_version APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_fx_dir}/other")

  # So does a driver whose version cannot be read.
  string(CONCAT _fx_pass
    "ERROR: Flang driver '[^']*/flang_exe/fail/flang' has LLVM major version "
    "unknown,\nbut SALT-FM was built with LLVM ${LLVM_VERSION_MAJOR}\\.")
  _salt_add_flang_exe_test(fparse_flang_exe_unreadable_version
    "${_fx_pass}" "cmd:"
    ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/fail/flang
    ${_fx_src})

  # No recorded driver and none on PATH.
  _salt_add_flang_exe_test(fparse_flang_exe_none_found
    "ERROR: no flang or flang-new on PATH" "cmd:;command not found"
    ${_fx_bin}/fparse-llvm-no-driver --show ${_fx_src})
  set_tests_properties(fparse_flang_exe_none_found PROPERTIES
    ENVIRONMENT "PATH=${_fx_dir}/nodriver")
  # saltfm finds fparse-llvm in its own directory without `dirname`,
  # also when its name has no directory.
  _salt_add_flang_exe_test(saltfm_no_dirname "USAGE: [^\n]*fparse-llvm"
    "command not found" ${_fx_bin}/saltfm --help-fortran)
  set_tests_properties(saltfm_no_dirname PROPERTIES
    ENVIRONMENT "PATH=${_fx_dir}/nodriver")
  _salt_add_flang_exe_test(saltfm_bare_name "USAGE: [^\n]*fparse-llvm"
    "command not found" ${_fx_dir}/nodriver/bash saltfm --help-fortran)
  set_tests_properties(saltfm_bare_name PROPERTIES
    WORKING_DIRECTORY ${_fx_bin})

  # A driver configure did not find is not recorded: no warning.
  _salt_add_flang_exe_test(fparse_flang_exe_no_record
    "${_fx_match_cmd}" "WARNING: recorded"
    ${_fx_bin}/fparse-llvm-no-record --show ${_fx_src})
  set_property(TEST fparse_flang_exe_no_record APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_fx_dir}/match")

  # --flang_exe (both forms, also through saltfm) and SALT_FLANG_EXE
  # select the driver; the flag has priority.
  _salt_add_flang_exe_test(fparse_flang_exe_flag
    "${_fx_match_cmd}" "${_fx_stub_ran};${_fx_note}"
    ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/match/flang
    ${_fx_src})
  _salt_add_flang_exe_test(fparse_flang_exe_flag_space
    "${_fx_match_cmd}" "${_fx_stub_ran};${_fx_note}"
    ${_fx_bin}/fparse-llvm --show --flang_exe ${_fx_dir}/match/flang
    ${_fx_src})
  _salt_add_flang_exe_test(saltfm_flang_exe_flag
    "${_fx_match_cmd}" "${_fx_stub_ran};${_fx_note}"
    ${_fx_bin}/saltfm --show --flang_exe=${_fx_dir}/match/flang ${_fx_src})
  _salt_add_flang_exe_test(fparse_flang_exe_env
    "${_fx_match_cmd}" "${_fx_stub_ran};${_fx_note}"
    ${_fx_bin}/fparse-llvm --show ${_fx_src})
  set_property(TEST fparse_flang_exe_env APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "SALT_FLANG_EXE=set:${_fx_dir}/match/flang")
  _salt_add_flang_exe_test(fparse_flang_exe_flag_over_env
    "${_fx_match_cmd}" "${_fx_stub_ran};ERROR"
    ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/match/flang
    ${_fx_src})
  set_property(TEST fparse_flang_exe_flag_over_env APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "SALT_FLANG_EXE=set:${_fx_dir}/fail/flang")
  # Only whole flags match, not names that contain one, such as `-S` or
  # `-W` here: the names are dropped, and a flag such as `-Iinc-Wave`,
  # which contains `-W`, is forwarded.
  set(_fx_flags "-fopenmp -Werror -Ifoo -Iinc-Wave -DBAR=1 -Jdir -std=f2018")
  string(CONCAT _fx_pass
    "Removed unknown flag: fortran_decoy-Stuff\\.f95\n"
    "Removed unknown flag: fortran_decoy-Wstuff\\.f95\n.*"
    "Remaining Arguments: ${_fx_flags}\n.*"
    "${_fx_match_cmd}[^\n]* ${_fx_flags}\n")
  _salt_add_flang_exe_test(fparse_whole_flag_names
    "${_fx_pass}" "cmd:[^\n]*decoy-"
    ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/match/flang
    -fopenmp -Werror -Wall -Ifoo -Iinc-Wave -DBAR=1 -Jdir -std=f2018
    -Wl,-rpath,x fortran_decoy-Stuff.f95 fortran_decoy-Wstuff.f95 ${_fx_src})
  # --flang_exe without a path stops.
  _salt_add_flang_exe_test(fparse_flang_exe_flag_no_path
    "ERROR: --flang_exe needs a path" "cmd:"
    ${_fx_bin}/fparse-llvm --show ${_fx_src} --flang_exe)
  _salt_add_flang_exe_test(fparse_flang_exe_flag_empty
    "ERROR: --flang_exe needs a path" "cmd:"
    ${_fx_bin}/fparse-llvm --show --flang_exe= ${_fx_src})
  # --version reports the selected driver, wherever the flag is.
  set(_fx_match_version
    "flang version ${LLVM_VERSION_MAJOR}\\.0\\.0 \\(stub\\)")
  _salt_add_flang_exe_test(fparse_flang_exe_version
    "${_fx_match_version}" "${_fx_stub_ran}"
    ${_fx_bin}/fparse-llvm --version --flang_exe=${_fx_dir}/match/flang)
  _salt_add_flang_exe_test(saltfm_flang_exe_version
    "${_fx_match_version}" "${_fx_stub_ran}"
    ${_fx_bin}/saltfm --lang=fortran --version
    --flang_exe ${_fx_dir}/match/flang)
  _salt_add_flang_exe_test(saltfm_flang_exe_version_before
    "ERROR: Flang driver '[^']*/flang_exe/fail/flang'" "flang version"
    ${_fx_bin}/saltfm --lang=fortran --flang_exe=${_fx_dir}/fail/flang
    --version)

  # Since LLVM 20 the driver is `flang`; an override naming `flang-new`
  # gets a note.
  if(LLVM_VERSION_MAJOR GREATER_EQUAL 20)
    _salt_add_flang_exe_test(fparse_flang_exe_flang_new_note
      "${_fx_note}" "${_fx_stub_ran}"
      ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/match/flang-new
      ${_fx_src})
    _salt_add_flang_exe_test(fparse_flang_exe_env_flang_new_note
      "${_fx_note}" "${_fx_stub_ran}"
      ${_fx_bin}/fparse-llvm --show ${_fx_src})
    set_property(TEST fparse_flang_exe_env_flang_new_note APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "SALT_FLANG_EXE=set:${_fx_dir}/match/flang-new")
  endif()

  foreach(test_source IN LISTS FORTRAN_TESTS_SOURCES_LIST)
    add_test(NAME instrument_${test_source}
      COMMAND
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
        ${CMAKE_SOURCE_DIR}/tests/fortran/${test_source}
    )
    set(_required_files
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm"
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm")
    set_tests_properties(instrument_${test_source}
      PROPERTIES
      REQUIRED_FILES "${_required_files}"
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      FIXTURES_SETUP instrument_${test_source}
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished"
    )
    _salt_in_test_output(instrument instrument_${test_source})
  endforeach()

  add_test(NAME check-internal-func.f90
    COMMAND ${CMAKE_COMMAND} -E cat internal-func.inst.F90
  )
  set_tests_properties(check-internal-func.f90
    PROPERTIES
    PASS_REGULAR_EXPRESSION "[\n\r]+[\t ]+&add "
    FIXTURES_REQUIRED instrument_internal-func.f90
    LABELS "lang:Fortran;phase:check"
  )

  # Implicit main programs (no program-stmt) have no name or source
  # line to attach to the timer.  The plugin synthesizes a fallback
  # name combining the input file's basename with a fixed
  # `_implicit_main_` marker, and uses the first-executable-stmt line
  # for the timer-name source range.  Pin both: the timer name must
  # contain `implicit-main._implicit_main_`, and the source-range
  # field must not start at line 0.
  add_test(NAME check_implicit_main_timer_name
    COMMAND ${CMAKE_COMMAND} -E cat implicit-main.inst.F90
  )
  set_tests_properties(check_implicit_main_timer_name
    PROPERTIES
    PASS_REGULAR_EXPRESSION "implicit-main\\._implicit_main_"
    FAIL_REGULAR_EXPRESSION "\\[\\{[^}]*\\} \\{0,"
    FIXTURES_REQUIRED instrument_implicit-main.f90
    LABELS "lang:Fortran;phase:check"
  )

  # Companion tests: the alternate-return values 1, 2, 3 must each
  # survive instrumentation (preserved verbatim in the synthesized
  # if-then-endif blocks for the if-stmt forms, and untouched on the
  # bare-return line).  Each value pins its own ctest because CMake's
  # PASS_REGULAR_EXPRESSION uses any-of semantics on a `;`-separated
  # list, not all-of.
  foreach(alt_value 1 2 3)
    add_test(NAME check_alternate-return-value-${alt_value}
      COMMAND ${CMAKE_COMMAND} -E cat alternate-return.inst.F90
    )
    set_tests_properties(check_alternate-return-value-${alt_value}
      PROPERTIES
      PASS_REGULAR_EXPRESSION "return ${alt_value}"
      FIXTURES_REQUIRED instrument_alternate-return.f90
      LABELS "lang:Fortran;phase:check"
    )
    _salt_in_test_output(instrument check_alternate-return-value-${alt_value})
  endforeach()
  _salt_in_test_output(instrument
    check-internal-func.f90 check_implicit_main_timer_name)

  # Per-form skip checks: each minimal source contains exactly one
  # pure / elemental / impure-elemental / module-procedure body and
  # nothing else.  The companion check_no_tau_<src> test reads back
  # the instrumented output and fails if any TAU_PROFILE_* call
  # appears, which is the structural assertion that the procedure
  # was skipped.  Less brittle than pinning warning text.
  set(FORTRAN_TESTS_PURE_SKIP_SOURCES
    pure-fn-only.f90
    elemental-fn-only.f90
    impure-elemental-only.f90
    mp-pure-only.f90
    mp-elemental-only.f90
  )

  foreach(test_source IN LISTS FORTRAN_TESTS_PURE_SKIP_SOURCES)
    get_filename_component(test_base_name ${test_source} NAME_WLE)
    get_filename_component(test_lang ${test_source} LAST_EXT)
    set(_inst_output ${test_base_name}.inst${test_lang})
    string(REPLACE "f90" "F90" _inst_output "${_inst_output}")

    add_test(NAME instrument_${test_source}
      COMMAND
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
        ${CMAKE_SOURCE_DIR}/tests/fortran/${test_source}
    )
    set(_required_files
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm"
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm")
    set_tests_properties(instrument_${test_source}
      PROPERTIES
      REQUIRED_FILES "${_required_files}"
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      FIXTURES_SETUP instrument_${test_source}
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished"
    )

    add_test(NAME check_no_tau_${test_source}
      COMMAND ${CMAKE_COMMAND} -E cat ${_inst_output}
    )
    set_tests_properties(check_no_tau_${test_source}
      PROPERTIES
      FAIL_REGULAR_EXPRESSION "tauProfileTimer;TAU_PROFILE"
      FIXTURES_REQUIRED instrument_${test_source}
      LABELS "lang:Fortran;phase:check"
    )
    _salt_in_test_output(instrument
      instrument_${test_source} check_no_tau_${test_source})
  endforeach()

  # OpenMP constructs opening a procedure body: with -fopenmp the
  # plugin takes the start line from the construct's parse-tree node,
  # so the timer start must sit directly above each listed directive.
  function(_salt_add_omp_start_tests src)
    get_filename_component(_base ${src} NAME_WLE)
    add_test(NAME instrument_${src}
      COMMAND
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm -fopenmp
        ${CMAKE_SOURCE_DIR}/tests/fortran/${src}
    )
    set(required_files
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm"
      "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm")
    set_tests_properties(instrument_${src}
      PROPERTIES
      REQUIRED_FILES "${required_files}"
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      FIXTURES_SETUP instrument_${src}
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished"
      FAIL_REGULAR_EXPRESSION "ERROR:"
    )
    _salt_in_test_output(instrument instrument_${src})
    foreach(directive IN LISTS ARGN)
      string(REPLACE " " "_" _name "${directive}")
      string(CONCAT _start_above_directive
        "TAU_PROFILE_START\\(tauProfileTimer\\)[\r\n]+"
        "#line [0-9]+ \"[^\"]*\"[\r\n]+"
        " *!\\$omp ${directive}[\r\n]")
      add_test(NAME check_omp_start_${_name}
        COMMAND ${CMAKE_COMMAND} -E cat ${_base}.inst.F90
      )
      set_tests_properties(check_omp_start_${_name}
        PROPERTIES
        PASS_REGULAR_EXPRESSION "${_start_above_directive}"
        FIXTURES_REQUIRED instrument_${src}
        LABELS "lang:Fortran;phase:check"
      )
      _salt_in_test_output(instrument check_omp_start_${_name})
    endforeach()
  endfunction()

  _salt_add_omp_start_tests(omp-first-construct.f90
    "parallel" "parallel do" "sections" "barrier" "critical" "atomic update")
  # Flang 19 does not parse the `nothing` utility directive.
  if(LLVM_VERSION_MAJOR GREATER_EQUAL 20)
    _salt_add_omp_start_tests(omp-nothing.f90 "nothing")
  endif()
  _salt_add_omp_start_tests(use-omp-lib.f90)

  # SALT-FM's copies of the compiler modules must not hide a user
  # module of the same name, whether its directory is given with -I
  # or -J. The fixture writes the user module with SALT-FM's suffix
  # into a fresh directory; Flang does not create the -J directory.
  set(_user_mod_dir ${SALT_TEST_OUTPUT_DIR}/user-mod)
  set(_required_files
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm"
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm")
  add_test(NAME rm_old_user_mod_dir
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${_user_mod_dir})
  add_test(NAME mkdir_user_mod_dir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_user_mod_dir})
  add_test(NAME make_user_iso_fortran_env_mod
    COMMAND
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm -J${_user_mod_dir}
      --tau_output=${_user_mod_dir}/user-iso-fortran-env.inst.F90
      ${CMAKE_SOURCE_DIR}/tests/fortran/user-iso-fortran-env.f90)
  set_tests_properties(
    rm_old_user_mod_dir mkdir_user_mod_dir make_user_iso_fortran_env_mod
    PROPERTIES
    LABELS "lang:Fortran;phase:setup")
  set_tests_properties(rm_old_user_mod_dir mkdir_user_mod_dir PROPERTIES
    FIXTURES_SETUP user-mod_dir)
  set_tests_properties(mkdir_user_mod_dir PROPERTIES
    DEPENDS rm_old_user_mod_dir)
  set_tests_properties(make_user_iso_fortran_env_mod PROPERTIES
    WORKING_DIRECTORY ${_user_mod_dir}
    FIXTURES_SETUP user_iso_fortran_env_mod
    FIXTURES_REQUIRED user-mod_dir
    REQUIRED_FILES "${_required_files}"
    ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
    PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
  # Instruments `src` with the user module directory given by `flag`.
  function(_salt_add_user_mod_test src flag)
    get_filename_component(base ${src} NAME_WLE)
    set(name instrument_${base}${flag})
    add_test(NAME ${name}
      COMMAND
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
        ${flag}${_user_mod_dir} --tau_output=${base}${flag}.inst.F90
        ${CMAKE_SOURCE_DIR}/tests/fortran/${src})
    set_tests_properties(${name} PROPERTIES
      FIXTURES_REQUIRED user_iso_fortran_env_mod
      REQUIRED_FILES "${_required_files}"
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
    _salt_in_test_output(instrument ${name})
  endfunction()
  _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -I)
  _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -J)
  _salt_add_user_mod_test(use-mixed-nature.f90 -I)

  _salt_add_output_error_tests(fortran hello.inst.F90 "lang:Fortran"
    ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  if(EXISTS /dev/full)
    _salt_add_output_error_test(full_fortran /dev/full "lang:Fortran"
      --tau_output=/dev/full ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  endif()
  _salt_add_clang_error_test(fortran_error_old_output
    fortran_error_old.inst.F90 "lang:Fortran" fortran/compile_error.f90)
  _salt_add_clang_error_test(fortran_error_dir_output
    fortran_error_dir.inst.F90 "lang:Fortran" fortran/compile_error.f90
    CHECK "-d fortran_error_dir.inst.F90")
  _salt_add_clang_error_test(fortran_error_self_output fortran_error_self.f90
    "lang:Fortran" ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_error_self.f90
    COPY_OF fortran/compile_error.f90)
  _salt_add_clang_error_test(fortran_hello_self_output fortran_hello_self.f90
    "lang:Fortran" ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_hello_self.f90
    COPY_OF fortran/hello.f90)
  _salt_add_clang_error_test(fortran_config_self_output
    fortran_config_self.yaml "lang:Fortran" fortran/hello.f90
    COPY_OF setup_timer_name.yaml ARGS --config_file=fortran_config_self.yaml)
  # A trailing `--tau_output` without a value must not skip the check.
  _salt_add_clang_error_test(fortran_config_tail_self_output
    fortran_config_tail_self.yaml "lang:Fortran" fortran/hello.f90
    COPY_OF setup_timer_name.yaml
    ARGS --config_file=fortran_config_tail_self.yaml TAIL --tau_output)
  _salt_add_clang_error_test(fortran_sif_self_output fortran_sif_self.tau
    "lang:Fortran" fortran/hello.f90 COPY_OF sif/exclude_f90.tau
    ARGS --tau_select_file=fortran_sif_self.tau)
  # The wrapper does not recognize a `.f95` source, but must refuse it too.
  _salt_add_clang_error_test(fortran_error_self_f95_output
    fortran_error_self.f95 "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_error_self.f95
    COPY_OF fortran/compile_error.f90 ARGS --lang=fortran)
  _salt_add_clang_error_test(fortran_error_ro_output
    fortran_error_ro.inst.F90 "lang:Fortran" fortran/hello.f90 NOT_ROOT
    CHECK "-f fortran_error_ro.inst.F90 -a ! -w fortran_error_ro.inst.F90")
  # The wrapper's own early exits must not leave a stale output either.
  _salt_add_clang_error_test(fortran_early_exit_output
    fortran_early_exit.inst.F90 "lang:Fortran" fortran/hello.f90
    ARGS --flang_exe=/nonexistent/flang)
  _salt_add_clang_error_test(fortran_early_exit_space_output
    fortran_early_exit_space.inst.F90 "lang:Fortran" fortran/hello.f90
    SPACE ARGS --flang_exe=/nonexistent/flang)
  # The second `--tau_output` is the selective instrumentation file, so the
  # source after it is an input that saltfm must refuse to overwrite.
  _salt_add_clang_error_test(fortran_select_value_self_output
    fortran_select_value_self.f90 "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_select_value_self.f90
    DEFAULT_OUTPUT COPY_OF fortran/hello.f90
    ARGS --flang_exe=/nonexistent/flang
    --tau_output=fortran_select_value_self.f90 --tau_select_file --tau_output)
  # The value of `-D` is not an input, so saltfm must not refuse an output
  # of the same name, and must remove the stale one.
  _salt_add_clang_error_test(fortran_forwarded_value_output
    fortran_forwarded_value "lang:Fortran" fortran/hello.f90
    ARGS -D fortran_forwarded_value --flang_exe=/nonexistent/flang)
  # Only a whole `-x` token takes a value, not a name that contains `-x`,
  # so the source after such a name is an input that saltfm must refuse
  # to overwrite.
  _salt_add_clang_error_test(fortran_dash_x_name_self_output
    fortran_dash_x_name_self.f90 "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_dash_x_name_self.f90
    DEFAULT_OUTPUT COPY_OF fortran/hello.f90
    ARGS --tau_output=fortran_dash_x_name_self.f90
    --flang_exe=/nonexistent/flang fortran_other-x.f95)

  set(SALT_FORTRAN_COMPILERS_TO_TEST gfortran flang-new)

  if(HAVE_TAU)
    set(TAU_F90_OPTS
      -optVerbose -optNoRevert -optSaltInst
      -optSaltParser=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm)
    if(APPLE)
      list(APPEND TAU_F90_OPTS -optShared)
    endif()
    foreach(test_source IN LISTS FORTRAN_TESTS_SOURCES_LIST)
      # Get the name of the instrumented source file
      get_filename_component(test_base_name ${test_source} NAME_WLE)
      get_filename_component(test_lang ${test_source} LAST_EXT)
      # fparse-llvm is adding preprocessor directives and should emit
      # uppercase file extensions (e.g., .F90)
      set(TEST_INST_SOURCE
        ${CMAKE_SOURCE_DIR}/tests/fortran/${test_base_name}${test_lang})

      foreach(compiler IN LISTS SALT_FORTRAN_COMPILERS_TO_TEST)
        _salt_compiler_to_toolchain(${compiler} tc_label)
        _salt_compiler_to_dir(${compiler} dir)
        string(TOUPPER ${compiler} upper_comp)
        if(${compiler} STREQUAL "gfortran")
          set(EXTRA_FLAGS
            -cpp -Wpedantic -Wextra -Wno-unused-function
            -Wno-missing-include-dirs ${SALT_WERROR})
        elseif(${compiler} MATCHES "^flang(-new)?$")
          set(EXTRA_FLAGS ${SALT_WERROR})
        else()
          message(FATAL_ERROR "Unknown compiler: ${compiler}")
        endif()
        # Sources that use obsolescent (but standard) features need
        # -Wpedantic and -Werror dropped so the warning does not block
        # the build.
        if(test_source IN_LIST FORTRAN_TESTS_PERMISSIVE_SOURCES)
          list(REMOVE_ITEM EXTRA_FLAGS "-Wpedantic" ${SALT_WERROR})
        endif()
        set(PROF_FIX clean_${upper_comp}_${test_source}_profiles)
        add_test(NAME compile_${upper_comp}_${test_source}
          COMMAND ${TAUF90} ${TAU_F90_OPTS} -o ${test_base_name}
            -Wall ${EXTRA_FLAGS} ${TEST_INST_SOURCE}
          WORKING_DIRECTORY ${SALT_TAU_TEST_DIR}/${dir}/fortran
        )
        set_tests_properties(compile_${upper_comp}_${test_source}
          PROPERTIES
          ENVIRONMENT "TAU_MAKEFILE=${TAU_${dir}_MAKEFILE}"
          ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${TAU_BIN_DIR}"
          FIXTURES_REQUIRED ${dir}_dir
          FIXTURES_SETUP compile_${upper_comp}_${test_source}
          LABELS "lang:Fortran;${tc_label};phase:compile"
          FAIL_REGULAR_EXPRESSION "[^W][Ee]rror"
        )
        _salt_tau_emit_profile_dir_fixture(
          ${upper_comp} ${test_source} ${test_base_name} ${dir}/fortran
          "lang:Fortran" "${tc_label}" ${PROF_FIX} ${dir}_dir)
        _salt_tau_emit_run_check(
          COMP_ID ${upper_comp}
          TEST_ID ${test_source}
          TEST_BASE ${test_base_name}
          DIR ${dir}/fortran
          BINARY ${test_base_name}
          LANG_LABEL "lang:Fortran"
          TC_LABEL "${tc_label}"
          TAU_MAKEFILE "${TAU_${dir}_MAKEFILE}"
          COMPILE_DEP compile_${upper_comp}_${test_source}
          PROFILE_FIX ${PROF_FIX})
      endforeach()
    endforeach()
  endif()
endif()

if(HAVE_TAU)
  # A SIF that excludes the main program, or an include list that leaves it
  # out, removes only main's timer: TAU's setup code stays, so the program
  # writes profile.0.0.0 when run without tau_exec, which would set the node
  # itself, and a threaded program has no timer errors (#71). Each test
  # builds `src` with `sif` for each compiler of its language in a fresh
  # <name>.d directory, runs it there, and checks that the profile matches
  # `pass`, such as keep_routine, but not main's timer, which `main_timer`
  # matches. The optional OUTPUT must match the program's output, and the
  # other arguments are extra compile options.
  function(_salt_add_tau_main_test name src sif pass main_timer)
    cmake_parse_arguments(PARSE_ARGV 5 ARG "" "OUTPUT" "")
    if(src MATCHES "[.]f90$")
      set(tauc ${TAUF90})
      set(compilers ${SALT_FORTRAN_COMPILERS_TO_TEST})
      set(lang_dir fortran)
    elseif(src MATCHES "[.]c$")
      set(tauc ${TAUCC})
      set(compilers ${SALT_COMPILERS_TO_TEST})
      set(lang_dir c)
    else()
      set(tauc ${TAUCXX})
      set(compilers ${SALT_COMPILERS_TO_TEST})
      set(lang_dir cxx)
    endif()
    _salt_ext_to_label(${src} lang_label)
    set(tauc_opts
      -optVerbose -optNoRevert -optSaltInst
      -optSaltParser=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
      -optTauSelectFile=${CMAKE_SOURCE_DIR}/${sif})
    if(APPLE)
      list(APPEND tauc_opts -optShared)
    endif()
    foreach(comp IN LISTS compilers)
      _salt_compiler_to_toolchain(${comp} tc_label)
      _salt_compiler_to_dir(${comp} dir)
      set(comp_opts "")
      if(comp STREQUAL "gfortran")
        set(comp_opts -cpp)
      endif()
      set(work_dir ${SALT_TAU_TEST_DIR}/${dir}/${lang_dir}/${name}.d)
      set(prof_fix clean_${comp}_${name}_profiles)
      _salt_tau_emit_profile_dir_fixture(
        ${comp} ${name} ${name} ${dir}/${lang_dir}
        "${lang_label}" "${tc_label}" ${prof_fix} ${dir}_dir)
      add_test(NAME compile_${comp}_${name}
        COMMAND ${tauc} ${tauc_opts} ${comp_opts} ${ARG_UNPARSED_ARGUMENTS}
          -o ${name} ${CMAKE_SOURCE_DIR}/${src}
        WORKING_DIRECTORY ${work_dir}
      )
      set_tests_properties(compile_${comp}_${name}
        PROPERTIES
        ENVIRONMENT TAU_MAKEFILE=${TAU_${dir}_MAKEFILE}
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${TAU_BIN_DIR}"
        FIXTURES_REQUIRED ${prof_fix}
        FIXTURES_SETUP compile_${comp}_${name}
        LABELS "${lang_label};${tc_label};phase:compile"
        FAIL_REGULAR_EXPRESSION
          "[Dd]isabling instrumentation;[Nn]on-[Ii]nstrumented;[Ee]rror;Reverting to a Regular Make"
      )
      add_test(NAME run_${comp}_${name}
        COMMAND ./${name}
        WORKING_DIRECTORY ${work_dir}
      )
      set_tests_properties(run_${comp}_${name}
        PROPERTIES
        ENVIRONMENT "PROFILEDIR=${work_dir}"
        FIXTURES_REQUIRED compile_${comp}_${name}
        FIXTURES_SETUP run_${comp}_${name}
        LABELS "${lang_label};${tc_label};phase:run"
        FAIL_REGULAR_EXPRESSION "[Ss]egmentation;[Ff]ault;[Ee]rror"
      )
      if(ARG_OUTPUT)
        set_tests_properties(run_${comp}_${name}
          PROPERTIES PASS_REGULAR_EXPRESSION "${ARG_OUTPUT}")
      endif()
      if(APPLE)
        # Without tau_exec, dyld must find TAU's dylibs by their bare install
        # names, in lib and in the shared-<variant> dir of the Makefile
        string(REPLACE "/Makefile.tau" "/shared" shared_dir "${TAU_${dir}_MAKEFILE}")
        set(dyld_mods
          "DYLD_LIBRARY_PATH=path_list_prepend:${TAU_ARCH_DIR}/lib"
          "DYLD_LIBRARY_PATH=path_list_prepend:${shared_dir}")
        set_tests_properties(run_${comp}_${name}
          PROPERTIES ENVIRONMENT_MODIFICATION "${dyld_mods}")
      endif()
      add_test(NAME check_${comp}_${name}_profile
        COMMAND ${CMAKE_COMMAND} -E cat profile.0.0.0
        WORKING_DIRECTORY ${work_dir}
      )
      set_tests_properties(check_${comp}_${name}_profile
        PROPERTIES
        PASS_REGULAR_EXPRESSION "${pass}"
        FAIL_REGULAR_EXPRESSION "${main_timer}"
        FIXTURES_REQUIRED run_${comp}_${name}
        LABELS "${lang_label};${tc_label};phase:check"
      )
    endforeach()
  endfunction()

  foreach(sif exclude_main include_list)
    _salt_add_tau_main_test(sif_${sif}_c
      tests/sif_excl.c tests/sif/${sif}.tau keep_routine "int main[(]")
    _salt_add_tau_main_test(sif_${sif}_threads
      tests/sif_main_threads.cpp tests/sif/${sif}.tau keep_routine "int main[(]" -pthread)
    if(APPLE)
      # On macOS, TAU numbers the main thread above 0 when worker threads
      # start timers first, so TauProfiler_StoreData returns early (TAU
      # src/Profile/Profiler.cpp:1720) and writes no profile
      foreach(comp IN LISTS SALT_COMPILERS_TO_TEST)
        set_tests_properties(check_${comp}_sif_${sif}_threads_profile
          PROPERTIES DISABLED TRUE)
      endforeach()
    endif()
    if(TEST_FORTRAN)
      _salt_add_tau_main_test(sif_${sif}_fortran
        tests/fortran/sif_excl.f90 tests/sif/${sif}.tau keep_routine
        "${_sif_main_timer_fortran}")
    endif()
  endforeach()
  # A source with main that the file lists exclude compiles without an
  # error, from SALT-FM's copy, whose #line 1 keeps __FILE__ the name of
  # the source. Its main has no timer but sets the node, so the program,
  # with the timer of keep_routine in sif_nomain.c, writes profile.0.0.0.
  foreach(ext c cpp)
    _salt_add_tau_main_test(sif_file_exclude_main_${ext}
      tests/sif_file_name.${ext} tests/sif/file_exclude_file_name.tau
      keep_routine "int main[(]"
      OUTPUT "file: [^\n]*/tests/sif_file_name[.]${ext}\n"
      ${CMAKE_SOURCE_DIR}/tests/sif_nomain.c)
  endforeach()
  if(TEST_FORTRAN)
    _salt_add_tau_main_test(sif_file_exclude_main_fortran
      tests/fortran/sif_file_name.f90 tests/sif/file_exclude_file_name.tau
      keep_routine "[Ss][Ii][Ff]_[Ff][Ii][Ll][Ee]_[Nn][Aa][Mm][Ee] .[{]"
      ${CMAKE_SOURCE_DIR}/tests/fortran/sif_keep.f90)
  endif()
endif()
