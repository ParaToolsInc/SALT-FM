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
# For salt-selectfile and sif_check, which need only LLVM's Support library
llvm_map_components_to_libnames(SALT_LLVM_SUPPORT_LIBS support)

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
  config_file.hpp
  dprint.hpp
  output_file.hpp
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
  sema_fabricator.cpp
)

list(TRANSFORM CPARSE_LLVM_SRCS PREPEND "${CMAKE_SOURCE_DIR}/src/")

# selectfile.cpp, config_file.cpp, and output_file.cpp, compiled once for
# cparse-llvm and the Flang plugin (thus position independent), and
# selectfile.cpp and output_file.cpp, which it uses, for salt-selectfile
# and sif_check too
foreach(name selectfile config_file output_file)
  add_library(salt_${name}_obj OBJECT src/${name}.cpp)
  target_include_directories(salt_${name}_obj PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${LLVM_INCLUDE_DIRS})
  target_compile_definitions(salt_${name}_obj PRIVATE ${LLVM_DEFINITIONS_LIST})
  target_compile_features(salt_${name}_obj PRIVATE cxx_std_17)
  target_compile_options(salt_${name}_obj PRIVATE
    ${USE_RTTI} -Wall -Wpedantic -Wno-unused-variable ${SALT_WERROR})
  set_target_properties(salt_${name}_obj PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
endforeach()

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
target_link_libraries(cparse-llvm PRIVATE salt_selectfile_obj salt_config_file_obj
  salt_output_file_obj)
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

# The shell code that saltfm and fparse-llvm share, without its shebang,
# for @SALT_COMMON_SH@ in their templates (#105)
set(_salt_common_sh ${CMAKE_SOURCE_DIR}/src/salt-common.sh)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_salt_common_sh})
file(READ ${_salt_common_sh} SALT_COMMON_SH)
string(REGEX REPLACE "^#![^\n]*\n" "" SALT_COMMON_SH "${SALT_COMMON_SH}")

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
    config_file.hpp
    dprint.hpp
    output_file.hpp
    selectfile.hpp
    flang_source_location.hpp
    flang_instrumentation_constants.hpp
    flang_instrumentation_point.hpp
  )
  list(TRANSFORM SALT_FLANG_PLUGIN_HEADER_FILES PREPEND "${CMAKE_SOURCE_DIR}/include/")

  set(SALT_FLANG_PLUGIN_SRCS
    dprint.cpp
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
  target_link_libraries(salt-flang-plugin PRIVATE salt_selectfile_obj
    salt_config_file_obj salt_output_file_obj)
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

  # Tells fparse-llvm, before Flang runs, whether the file lists exclude a
  # source, and copies a source
  add_executable(salt-selectfile src/selectfile_main.cpp)
  target_link_libraries(salt-selectfile PRIVATE salt_selectfile_obj
    salt_output_file_obj ${SALT_LLVM_SUPPORT_LIBS})
  target_include_directories(salt-selectfile PRIVATE
    ${CMAKE_SOURCE_DIR}/include ${LLVM_INCLUDE_DIRS})
  target_compile_definitions(salt-selectfile PRIVATE ${LLVM_DEFINITIONS_LIST})
  target_compile_features(salt-selectfile PRIVATE cxx_std_17)
  target_compile_options(salt-selectfile PRIVATE
    ${USE_RTTI} -Wall -Wpedantic -Wno-unused-variable ${SALT_WERROR})
  set_target_properties(salt-selectfile PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}")
  install(TARGETS salt-selectfile DESTINATION ${CMAKE_INSTALL_BINDIR})

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
  # For the tests that run the plugin without fparse-llvm
  set(_salt_test_flang "${_salt_flang_driver}")
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

# Configure inlines src/salt-common.sh into the scripts (#105).
set(_salt_common_scripts saltfm)
if(TEST_FORTRAN)
  list(APPEND _salt_common_scripts fparse-llvm)
endif()
foreach(script IN LISTS _salt_common_scripts)
  add_test(NAME salt_common_sh_${script}
    COMMAND grep -q "^function env_bool "
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/${script})
  set_tests_properties(salt_common_sh_${script} PROPERTIES
    LABELS "phase:smoke")
endforeach()

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
# Adds output_error_<name>: saltfm, run in test-output/output_error with
# the other arguments, must fail, naming `file` in an error that holds
# TEXT, if given.
function(_salt_add_output_error_test name file label)
  cmake_parse_arguments(PARSE_ARGV 3 ARG "" "TEXT" "")
  add_test(NAME output_error_${name}
    COMMAND ${CMAKE_COMMAND} -DFILE=${file} -DTEXT=${ARG_TEXT}
      -P ${CMAKE_SOURCE_DIR}/tests/expect_output_error.cmake --
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
      ${ARG_UNPARSED_ARGUMENTS})
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
    TEXT "Could not create a temporary file next to output file"
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
# removes the `new`, fabricated, missing header, multi-source, and
# plugin_default outputs, makes empty temp_output, temp%output,
# write_error, stdout_output, and plugin_default_cwd directories and
# write_error_percent ones that hold only an empty a%2Fb directory,
# copies missing_header.c and the source of plugin_default to the
# working directory, writes stale `old`, `early_exit`, and
# `forwarded_value` ones that saltfm must remove, makes the `dir` ones
# empty directories that saltfm must keep, copies the bad and good
# sources and a config and a selective instrumentation file to `self`
# ones that saltfm must refuse and keep when they are also the output,
# makes read-only `ro` outputs that saltfm must not remove and then fail
# to open for writing, makes a configuration file that saltfm cannot
# read, copies the sources of the no_path_tau_output, `self`, and `rsp`
# tests, writes a response file naming the `rsp` one, and writes stale
# outputs of the other no_path tests.
set(_tests ${CMAKE_SOURCE_DIR}/tests)
add_test(NAME make_clang_error_outputs
  COMMAND sh -c "rm -rf clang_error_new.inst.c fabricate_on.inst.cpp &&
    rm -f missing_header_fails.inst.c missing_header.inst.c &&
    rm -f allow_parse_errors_missing_input.inst.c &&
    rm -f allow_parse_errors_c.inst.c allow_parse_errors_env.inst.c &&
    rm -f allow_parse_errors_cxx.inst.cpp fortran_default_output_* &&
    rm -f data_only.saltmod sif_nomain.inst.c* plugin_default.inst.F90* &&
    cp ${_tests}/fortran/hello.f90 plugin_default.f90 &&
    rm -rf temp_output_c temp_output_fortran temp%output &&
    mkdir temp_output_c temp_output_fortran temp%output &&
    rm -rf write_error_c write_error_fortran write_error_fortran_copy &&
    mkdir write_error_c write_error_fortran write_error_fortran_copy &&
    rm -rf write_error_percent_c write_error_percent_fortran &&
    mkdir -p write_error_percent_c/a%2Fb write_error_percent_fortran/a%2Fb &&
    rm -rf stdout_output_fortran plugin_default_cwd &&
    mkdir stdout_output_fortran plugin_default_cwd &&
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
    cp ${_tests}/fortran/compile_error.f90 fortran_error_self.inc &&
    cp ${_tests}/hello.c clang_hello_self.c &&
    cp ${_tests}/setup_timer_name.yaml clang_config_self.yaml &&
    cp ${_tests}/sif/exclude_c.tau clang_sif_self.tau &&
    cp ${_tests}/setup_timer_name.yaml fortran_config_self.yaml &&
    cp ${_tests}/setup_timer_name.yaml fortran_config_tail_self.yaml &&
    cp ${_tests}/sif/exclude_f90.tau fortran_sif_self.tau &&
    cp ${_tests}/fortran/hello.f90 fortran_hello_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_select_value_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_dash_x_name_self.f90 &&
    cp ${_tests}/fortran/hello.f90 fortran_other-x.inc &&
    rm -f config_unreadable.yaml &&
    cp ${_tests}/setup_timer_name.yaml config_unreadable.yaml &&
    chmod a-r config_unreadable.yaml &&
    rm -f no_path_* &&
    cp ${_tests}/hello.c no_path_tau_output_empty_c.c &&
    cp ${_tests}/hello.c no_path_tau_output_tail_c.c &&
    cp ${_tests}/fortran/hello.f90 no_path_tau_output_empty_fortran.f90 &&
    cp ${_tests}/fortran/hello.f90 no_path_tau_output_tail_fortran.f90 &&
    for opt in config_file flang_exe tau_select_file; do
      for end in empty_c tail_c empty_fortran tail_fortran; do
        echo stale > no_path_\${opt}_\${end}.out || exit 1; done; done &&
    echo stale > no_path_space_c.out &&
    echo stale > no_path_dash_space_c.out &&
    echo stale > no_path_o_c.out &&
    echo stale > no_path_dashes_o_c.out &&
    rm -f o_* multi_source_one.inst.* multi_source_no_output_* tau_output_twice_* &&
    rm -f multi_source_lang_* repeated_source_fortran.* unknown_value_fortran.* &&
    cp ${_tests}/fortran/hello.f90 multi_source_no_output_a.f90 &&
    cp ${_tests}/fortran/cubes.f multi_source_no_output_b.f &&
    cp ${_tests}/fortran/hello.f90 multi_source_lang_a.inc &&
    cp ${_tests}/fortran/hello.f90 multi_source_lang_b.inc &&
    cp ${_tests}/fortran/hello.f90 repeated_source_fortran.f90 &&
    cp ${_tests}/fortran/hello.f90 unknown_value_fortran.inc &&
    cp ${_tests}/hello.c o_after_dashes_c.c &&
    cp ${_tests}/fortran/hello.f90 o_after_dashes_fortran.f90 &&
    echo stale > o_joined_fortran.out &&
    cp ${_tests}/hello.c no_path_self_c.c &&
    cp ${_tests}/hello.c no_path_rsp_c.c &&
    echo no_path_rsp_c.c > no_path_rsp_c.rsp &&
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
# arguments follow `src`. Each LOG text must be in saltfm's output, no
# NOT_LOG text, and each OUT text in `out`.
# COPY_OF `fixture` (relative to tests/) instead expects the error that
# `out`, a copy of the fixture that is also `src` or another input, is
# an input file, and `out` unchanged.
function(_salt_add_clang_error_test name out labels src)
  cmake_parse_arguments(PARSE_ARGV 4 ARG "EXIT_ZERO;NOT_ROOT;SPACE;DEFAULT_OUTPUT"
    "CHECK;COPY_OF" "ARGS;TAIL;LOG;NOT_LOG;OUT")
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
  foreach(text IN LISTS ARG_NOT_LOG)
    string(APPEND text_checks "! grep -qF -- \"${text}\" ${log_file} ||
      { echo \"Did not expect ${text} in ${log_file}\"; exit 1; }\n")
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
# fails even for a good source, and the file does not change.
_salt_add_clang_error_test(clang_error_ro_output clang_error_ro.inst.c
  "lang:C" hello.c NOT_ROOT
  CHECK "-f clang_error_ro.inst.c -a ! -w clang_error_ro.inst.c"
  LOG "ERROR: Could not open output file clang_error_ro.inst.c" OUT stale)
# saltfm writes a device output such as /dev/null directly: a rename
# onto it fails, or, as root, replaces the device.
_salt_add_clang_error_test(dev_null_c /dev/null "lang:C" hello.c EXIT_ZERO
  CHECK "-c /dev/null")
# With more than one source, --tau_output is an error before a parse,
# which would report the errors in clang_error.c.
_salt_add_clang_error_test(multi_source_one_output multi_source_one.inst.c
  "lang:C" hello.c TAIL ${CMAKE_SOURCE_DIR}/tests/clang_error.c
  LOG "ERROR: -o or --tau_output needs exactly one source file; got 2"
  NOT_LOG "error:")
# -o names the output as --tau_output does, before or after the source,
# but not after `--`, where it is a build option. With --tau_output, it
# is an error.
_salt_add_clang_error_test(o_before_source_c o_before_source_c.out "lang:C"
  hello.c DEFAULT_OUTPUT EXIT_ZERO CHECK "-s o_before_source_c.out"
  ARGS -o o_before_source_c.out OUT "TAU_PROFILE_START")
_salt_add_clang_error_test(o_after_source_c o_after_source_c.out "lang:C"
  hello.c DEFAULT_OUTPUT EXIT_ZERO CHECK "-s o_after_source_c.out"
  TAIL -o o_after_source_c.out OUT "TAU_PROFILE_START")
# Guard: fails if the option parser read -o after `--` as the output.
_salt_add_clang_error_test(o_after_dashes_c o_after_dashes_c.inst.c "lang:C"
  ${SALT_TEST_OUTPUT_DIR}/output_error/o_after_dashes_c.c DEFAULT_OUTPUT
  EXIT_ZERO CHECK "-s o_after_dashes_c.inst.c -a ! -e o_after_dashes_c.o"
  TAIL -- -o o_after_dashes_c.o OUT "TAU_PROFILE_START")
_salt_add_clang_error_test(o_with_tau_output_c o_with_tau_output_c.out
  "lang:C" hello.c CHECK "! -e o_with_tau_output_c.out -a ! -e o_with_tau_output_c.o"
  ARGS -o o_with_tau_output_c.o LOG "ERROR: give -o or --tau_output only once")
# So is --tau_output given twice.
_salt_add_clang_error_test(tau_output_twice_c tau_output_twice_c.out "lang:C"
  hello.c CHECK "! -e tau_output_twice_c.out -a ! -e tau_output_twice_c.first"
  ARGS --tau_output=tau_output_twice_c.first
  LOG "ERROR: give -o or --tau_output only once")
# C++ reads -o as C does.
_salt_add_clang_error_test(o_before_source_cxx o_before_source_cxx.out
  "lang:CXX" c11test.cpp DEFAULT_OUTPUT EXIT_ZERO
  CHECK "-s o_before_source_cxx.out" ARGS -o o_before_source_cxx.out
  OUT "TAU_PROFILE_START")
# Adds o_<form>_<name>: saltfm on `src` (relative to tests/) with the
# output o_<form>_<name><ext> given as -o=<file>, --o <file>, or
# --o=<file>, which LLVM's option parser also reads as -o <file>.
function(_salt_add_o_form_tests name labels src ext)
  foreach(form IN ITEMS equals dashes dashes_equals)
    set(out o_${form}_${name}${ext})
    set(args_equals -o=${out})
    set(args_dashes --o ${out})
    set(args_dashes_equals --o=${out})
    _salt_add_clang_error_test(o_${form}_${name} ${out} "${labels}" ${src}
      DEFAULT_OUTPUT EXIT_ZERO CHECK "-s ${out}" ARGS ${args_${form}}
      OUT "TAU_PROFILE_START")
  endforeach()
endfunction()
_salt_add_o_form_tests(c "lang:C" hello.c .out)
# Adds temp_output_<lang>: saltfm with umask 022, run in the setup's
# empty directory temp_output_<lang> on the sources in ARGN, must exit
# with status 0 and leave only their default outputs `outs`, each with
# mode 0644 and a timer, and no temporary file.
function(_salt_add_temp_output_test lang labels outs)
  string(REPLACE ";" " " outs "${outs}")
  add_test(NAME temp_output_${lang}
    COMMAND sh -c "cd temp_output_${lang} && umask 022 && \"$0\" \"$@\" || exit 1
      [ \"$(echo $(ls))\" = \"${outs}\" ] ||
        { echo \"Expected only ${outs}:\"; ls; exit 1; }
      for out in ${outs}; do
        [ \"$(ls -l $out | cut -c1-10)\" = -rw-r--r-- ] ||
          { echo \"Expected mode 0644:\"; ls -l $out; exit 1; }
        grep -q TAU_PROFILE_START $out ||
          { echo \"Expected a timer in $out\"; exit 1; }
      done"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm ${ARGN})
  set_tests_properties(temp_output_${lang} PROPERTIES
    FIXTURES_REQUIRED clang_error_outputs
    LABELS "${labels};phase:instrument")
  _salt_in_test_output(output_error temp_output_${lang})
endfunction()
_salt_add_temp_output_test(c "lang:C" "hello.inst.c;sif_nomain.inst.c"
  ${CMAKE_SOURCE_DIR}/tests/hello.c ${CMAKE_SOURCE_DIR}/tests/sif_nomain.c)
# An output in a directory whose path contains `%` also gets a temporary
# file, which leaves no file after the rename.
_salt_add_clang_error_test(percent_dir_output_c temp%output/hello.inst.c
  "lang:C" hello.c EXIT_ZERO
  CHECK "-s temp%output/hello.inst.c -a \"$(ls temp%output)\" = hello.inst.c"
  OUT TAU_PROFILE_START)
# Adds `name`: saltfm on `src`, run in the setup's empty directory
# `name` with --tau_output=`out`, the other arguments, and writes to
# regular files failing as on a full disk (`ulimit -f 0`, and SIGXFSZ
# ignored so that the write fails instead of killing saltfm), must exit
# with a status from 1 to 127 and an error naming `out`, and leave no
# file. LLVM's SIGXFSZ handler removes the temporary file itself, so
# these tests pass without the removal at exit too.
function(_salt_add_write_error_test name labels src out)
  add_test(NAME ${name}
    COMMAND sh -c "cd ${name} || exit 1
      log=$( (trap '' XFSZ; ulimit -f 0; exec \"$0\" \"$@\") 2>&1 ); rc=$?
      echo \"$log\"
      [ $rc -ge 1 ] && [ $rc -le 127 ] ||
        { echo \"Expected an exit status from 1 to 127, got $rc\"; exit 1; }
      echo \"$log\" | grep -qF 'ERROR: Could not write output file ${out}' ||
        { echo 'Expected the write error'; exit 1; }
      [ -z \"$(find . -type f)\" ] ||
        { echo 'Expected no file:'; find . -type f; exit 1; }"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --tau_output=${out}
      ${ARGN} ${src})
  set_tests_properties(${name} PROPERTIES
    FIXTURES_REQUIRED clang_error_outputs
    LABELS "${labels};phase:instrument")
  _salt_in_test_output(output_error ${name})
endfunction()
_salt_add_write_error_test(write_error_c "lang:C"
  ${CMAKE_SOURCE_DIR}/tests/hello.c write_error.inst.c)
# Also in a directory whose path contains `%`
_salt_add_write_error_test(write_error_percent_c "lang:C"
  ${CMAKE_SOURCE_DIR}/tests/hello.c a%2Fb/write_error.inst.c)
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

# A bad configuration file stops saltfm with an error that names the file
# and the problem, and leaves no output, in Debug and Release builds
# (#51, #54, #83, #90). config_errors/no_<key>.yaml in the build
# directory is the TAU configuration file without `key` and its entries.
set(_config_errors ${CMAKE_BINARY_DIR}/config_errors)
set(_tau_config_path ${CMAKE_SOURCE_DIR}/config_files/tau_config.yaml)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_tau_config_path})
file(READ ${_tau_config_path} _tau_config)
# Sets `out` to `input` with `from` replaced by `to`, with
# string(REGEX REPLACE) when `mode` is REGEX, else string(REPLACE). Stops
# when nothing matched, so a change to tau_config.yaml cannot leave a
# fixture that tests nothing.
function(_salt_config_edit mode from to input out)
  if(mode STREQUAL "REGEX")
    string(REGEX REPLACE "${from}" "${to}" result "${input}")
  else()
    string(REPLACE "${from}" "${to}" result "${input}")
  endif()
  if(result STREQUAL input)
    message(FATAL_ERROR "No match in ${_tau_config_path} for:\n${from}")
  endif()
  set(${out} "${result}" PARENT_SCOPE)
endfunction()
foreach(key instrumentation include main_insert main_insert_scope
    function_begin_insert function_begin_insert_scope function_end_insert)
  _salt_config_edit(REGEX "\n${key}:[^\n]*(\n  [^\n]*)*" "" "${_tau_config}" _config)
  file(WRITE ${_config_errors}/no_${key}.yaml "${_config}")
endforeach()
_salt_config_edit(REGEX "\nFortran:.*" "\n" "${_tau_config}" _config)
file(WRITE ${_config_errors}/no_Fortran.yaml "${_config}")
file(WRITE ${_config_errors}/scalar_Fortran.yaml "${_config}Fortran: x\n")
foreach(key program_insert procedure_begin_insert procedure_end_insert)
  _salt_config_edit(REGEX "\n  ${key}:[^\n]*(\n    [^\n]*)*" "" "${_tau_config}" _config)
  file(WRITE ${_config_errors}/no_${key}.yaml "${_config}")
endforeach()
# config_errors/cxx_api.yaml has only the keys that --tau_use_cxx_api
# uses, and fortran.yaml only `Fortran`.
set(_config "${_tau_config}")
foreach(key main_insert function_begin_insert function_end_insert)
  _salt_config_edit(REGEX "\n${key}:[^\n]*(\n  [^\n]*)*" "" "${_config}" _config)
endforeach()
file(WRITE ${_config_errors}/cxx_api.yaml "${_config}")
string(REGEX MATCH "Fortran:.*" _config "${_tau_config}")
if(_config STREQUAL "")
  message(FATAL_ERROR "'Fortran:' not found in ${_tau_config_path}")
endif()
file(WRITE ${_config_errors}/fortran.yaml "${_config}")
# config_errors/<name>.yaml is the TAU configuration file with one value
# changed: from `_from_<name>` to `_to_<name>`.
set(_include "include:\n  - <Profile/Profiler.h>")
set(_from_scalar_include "${_include}")
set(_to_scalar_include "include: <Profile/Profiler.h>")
set(_from_empty_include "${_include}")
set(_to_empty_include "include: []")
set(_from_null_include "${_include}")
set(_to_null_include "include:")
set(_from_alias_include "${_include}")
set(_to_alias_include "headers: &x\n  - <Profile/Profiler.h>\ninclude: *x")
set(_from_undefined_alias_include "${_include}")
set(_to_undefined_alias_include "include: *nope")
set(_from_list_instrumentation "instrumentation: TAU")
set(_to_list_instrumentation "instrumentation: [TAU]")
set(_from_map_main_setup_insert "main_setup_insert:\n  - ")
set(_to_map_main_setup_insert "main_setup_insert:\n  - setup: ")
set(_from_map_function_end_insert "\n  - \"TAU_PROFILE_STOP")
set(_to_map_function_end_insert "\n  - stop: \"TAU_PROFILE_STOP")
set(_from_scalar_procedure_end_insert "procedure_end_insert:\n    - ")
set(_to_scalar_procedure_end_insert "procedure_end_insert: ")
set(_from_map_program_setup_insert "program_setup_insert:\n    - ")
set(_to_map_program_setup_insert "program_setup_insert:\n    - setup: ")
foreach(name scalar_include empty_include null_include alias_include
    undefined_alias_include list_instrumentation map_main_setup_insert
    map_function_end_insert scalar_procedure_end_insert
    map_program_setup_insert)
  _salt_config_edit(STRING "${_from_${name}}" "${_to_${name}}" "${_tau_config}" _config)
  file(WRITE ${_config_errors}/${name}.yaml "${_config}")
endforeach()
# The TAU configuration file as the one document of a YAML stream, and a
# stream of two documents
file(WRITE ${_config_errors}/doc_start.yaml "---\n${_tau_config}")
file(WRITE ${_config_errors}/two_docs.yaml "---\n${_tau_config}---\n${_tau_config}")
# A configuration file that does not parse: line 4 is a key in a list.
set(_config_parse_error ${_config_errors}/parse_error.yaml)
file(WRITE ${_config_parse_error}
  "instrumentation: TAU\ninclude:\n  - <Profile/Profiler.h>\n  main_insert:\n    - x\n")
# Adds config_error_<name>: saltfm on `src` (relative to tests/) with
# --config_file=`config` and the other ARGN must fail with `log` and
# leave no output.
function(_salt_add_config_error_test name labels src config log)
  _salt_add_clang_error_test(config_error_${name} config_error_${name}.out
    "${labels}" ${src} ARGS --config_file=${config} ${ARGN} LOG "${log}")
endfunction()
# Adds the config_error_<name>_<lang> tests for a missing, an unreadable,
# and an unparsable configuration file, one with an undefined YAML alias,
# and one with two YAML documents; config_ok_doc_start_<lang> for one
# document after `---`; and config_env_<lang>, config_env_option_<lang>,
# and config_env_empty_<lang>: without --config_file, SALT_CONFIG_FILE
# selects the file unless it is empty, and --config_file has priority
# over it.
function(_salt_add_config_read_error_tests lang labels src)
  set(missing ${_config_errors}/no_such_file.yaml)
  _salt_add_config_error_test(missing_${lang} "${labels}" ${src}
    ${missing} "ERROR: Could not read configuration file ${missing}")
  _salt_add_clang_error_test(config_env_${lang} config_env_${lang}.out
    "${labels}" ${src} LOG "ERROR: Could not read configuration file ${missing}")
  _salt_add_clang_error_test(config_env_option_${lang}
    config_env_option_${lang}.out "${labels}" ${src} EXIT_ZERO
    CHECK "-s config_env_option_${lang}.out"
    ARGS --config_file=${_tau_config_path})
  set_property(TEST config_env_${lang} config_env_option_${lang} APPEND
    PROPERTY ENVIRONMENT "SALT_CONFIG_FILE=${missing}")
  _salt_add_clang_error_test(config_env_empty_${lang}
    config_env_empty_${lang}.out "${labels}" ${src} EXIT_ZERO
    CHECK "-s config_env_empty_${lang}.out")
  set_property(TEST config_env_empty_${lang} APPEND
    PROPERTY ENVIRONMENT "SALT_CONFIG_FILE=")
  _salt_add_clang_error_test(config_ok_doc_start_${lang}
    config_ok_doc_start_${lang}.out "${labels}" ${src} EXIT_ZERO
    CHECK "-s config_ok_doc_start_${lang}.out"
    ARGS --config_file=${_config_errors}/doc_start.yaml OUT "TAU_PROFILE_START")
  set(two_docs ${_config_errors}/two_docs.yaml)
  _salt_add_config_error_test(two_docs_${lang} "${labels}" ${src} ${two_docs}
    "ERROR: The configuration file ${two_docs} must hold one YAML document")
  # Root can read the file.
  _salt_add_config_error_test(unreadable_${lang} "${labels}" ${src}
    config_unreadable.yaml
    "ERROR: Could not read configuration file config_unreadable.yaml" NOT_ROOT)
  _salt_add_config_error_test(parse_${lang} "${labels}" ${src}
    ${_config_parse_error} "ERROR: ${_config_parse_error}:4:3: parse error")
  set(alias ${_config_errors}/undefined_alias_include.yaml)
  _salt_add_config_error_test(undefined_alias_${lang} "${labels}" ${src}
    ${alias} "ERROR: ${alias}: anchor not found: 'nope'")
endfunction()
_salt_add_config_read_error_tests(c "lang:C" hello.c)
# With more than one source and no --tau_output, an error in a later
# source leaves no output from an earlier one (#104): saltfm copies
# sif_nomain.c, which the file lists exclude, and then stops on the
# configuration file, which it reads for the setup code of the main
# program in hello.c.
_salt_add_clang_error_test(multi_source_config_error sif_nomain.inst.c
  "lang:C" sif_nomain.c DEFAULT_OUTPUT
  CHECK "! -e sif_nomain.inst.c -a ! -e sif_nomain.inst.c-*.tmp"
  ARGS --config_file=${_config_parse_error}
  --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau
  --tau_excluded_main_setup=file
  TAIL ${CMAKE_SOURCE_DIR}/tests/hello.c
  LOG "ERROR: ${_config_parse_error}:4:3: parse error")
# Adds the no_path_<option>_<form>_<lang> tests: an option that takes a
# path, with an empty value (`empty`) or at the end without one (`tail`),
# stops saltfm with an error and no output. A --tau_output test runs on
# the setup's copy of `src` (relative to tests/), whose default output
# it checks; the others check that saltfm removes the setup's stale
# --tau_output file.
function(_salt_add_no_path_tests lang labels src)
  set(inst_ext .inst.c)
  if(lang STREQUAL "fortran")
    set(inst_ext .inst.F90)
  endif()
  cmake_path(GET src EXTENSION src_ext)
  foreach(opt IN ITEMS config_file flang_exe tau_output tau_select_file)
    foreach(form IN ITEMS empty tail)
      set(name no_path_${opt}_${form}_${lang})
      set(log "ERROR: --${opt} needs a path")
      if(opt STREQUAL "tau_output")
        set(log "ERROR: -o or --tau_output needs a path")
      endif()
      set(args ARGS --${opt}=)
      if(form STREQUAL "tail")
        set(args TAIL --${opt})
        # LLVM's option parser reports a missing value for C and C++
        if(lang STREQUAL "c")
          set(log "for the --${opt} option: requires a value")
        endif()
      endif()
      if(opt STREQUAL "tau_output")
        _salt_add_clang_error_test(${name} ${name}${inst_ext} "${labels}"
          ${SALT_TEST_OUTPUT_DIR}/output_error/${name}${src_ext} DEFAULT_OUTPUT
          ${args} LOG "${log}")
      else()
        _salt_add_clang_error_test(${name} ${name}.out "${labels}" ${src}
          ${args} LOG "${log}")
      endif()
    endforeach()
  endforeach()
endfunction()
_salt_add_no_path_tests(c "lang:C" hello.c)
# C and C++ ignore the --flang_exe path, given as `--flang_exe=<path>`
# or as `--flang_exe <path>`.
foreach(form IN ITEMS equals space)
  set(_name flang_exe_ignored_${form}_c)
  set(_flang_args --flang_exe=/no/such/flang)
  if(form STREQUAL "space")
    set(_flang_args --flang_exe /no/such/flang)
  endif()
  _salt_add_clang_error_test(${_name} ${_name}.out "lang:C" hello.c EXIT_ZERO
    CHECK "-s ${_name}.out" ARGS ${_flang_args} OUT "TAU_PROFILE_START")
endforeach()
# When an option error stops C and C++, saltfm also removes a stale
# output that a separate --tau_output, -tau_output, -o, or --o names,
# and keeps an output that is a source, also one in a response file.
_salt_add_clang_error_test(no_path_space_c no_path_space_c.out "lang:C"
  hello.c SPACE TAIL --tau_select_file LOG "requires a value")
_salt_add_clang_error_test(no_path_dash_space_c no_path_dash_space_c.out
  "lang:C" hello.c DEFAULT_OUTPUT ARGS -tau_output no_path_dash_space_c.out
  TAIL --tau_select_file LOG "requires a value")
_salt_add_clang_error_test(no_path_o_c no_path_o_c.out "lang:C" hello.c
  DEFAULT_OUTPUT ARGS -o no_path_o_c.out TAIL --tau_select_file
  LOG "requires a value")
_salt_add_clang_error_test(no_path_dashes_o_c no_path_dashes_o_c.out "lang:C"
  hello.c DEFAULT_OUTPUT ARGS --o no_path_dashes_o_c.out
  TAIL --tau_select_file LOG "requires a value")
_salt_add_clang_error_test(no_path_self_c no_path_self_c.c "lang:C"
  ${SALT_TEST_OUTPUT_DIR}/output_error/no_path_self_c.c COPY_OF hello.c
  TAIL --tau_select_file LOG "requires a value")
_salt_add_clang_error_test(no_path_rsp_c no_path_rsp_c.c "lang:C" hello.c
  COPY_OF hello.c ARGS @no_path_rsp_c.rsp TAIL --tau_select_file
  LOG "requires a value")
# C and C++ use `instrumentation` only in a log line, so it is optional.
_salt_add_clang_error_test(config_ok_no_instrumentation_c
  config_ok_no_instrumentation_c.out "lang:C" hello.c EXIT_ZERO
  CHECK "-s config_ok_no_instrumentation_c.out"
  ARGS --config_file=${_config_errors}/no_instrumentation.yaml
  OUT "TAU_PROFILE_START")
foreach(key include main_insert function_begin_insert function_end_insert)
  _salt_add_config_error_test(no_${key}_c "lang:C" hello.c
    ${_config_errors}/no_${key}.yaml
    "ERROR: '${key}' key not found in the configuration file")
endforeach()
# --tau_use_cxx_api uses the `_scope` keys.
foreach(key main_insert_scope function_begin_insert_scope)
  _salt_add_config_error_test(no_${key}_cxx "lang:CXX" c11test.cpp
    ${_config_errors}/no_${key}.yaml
    "ERROR: '${key}' key not found in the configuration file"
    --tau_use_cxx_api)
endforeach()
# A value that SALT-FM cannot use is an error too.
set(_in_config "in the configuration file ${_config_errors}")
set(_list "must be a list of strings")
_salt_add_config_error_test(scalar_include_c "lang:C" hello.c
  ${_config_errors}/scalar_include.yaml
  "ERROR: 'include' ${_in_config}/scalar_include.yaml ${_list}")
_salt_add_config_error_test(map_function_end_insert_c "lang:C" hello.c
  ${_config_errors}/map_function_end_insert.yaml
  "ERROR: 'function_end_insert' ${_in_config}/map_function_end_insert.yaml ${_list}")
_salt_add_config_error_test(map_main_setup_insert_c "lang:C" hello.c
  ${_config_errors}/map_main_setup_insert.yaml
  "ERROR: 'main_setup_insert' ${_in_config}/map_main_setup_insert.yaml ${_list}")
_salt_add_config_error_test(list_instrumentation_c "lang:C" hello.c
  ${_config_errors}/list_instrumentation.yaml
  "ERROR: 'instrumentation' ${_in_config}/list_instrumentation.yaml must be a string")
# A run needs only the keys that it uses: an empty list is valid, and a C
# run needs no `_scope` keys without --tau_use_cxx_api, no
# main_setup_insert, and no `Fortran` section, which the NVTX and ROCTX
# configuration files lack (main_excluded_<cfg>_c covers perfstubs and
# ITT).
foreach(name empty_include null_include)
  _salt_add_clang_error_test(config_ok_${name}_c config_ok_${name}_c.out
    "lang:C" hello.c EXIT_ZERO CHECK "-s config_ok_${name}_c.out"
    ARGS --config_file=${_config_errors}/${name}.yaml LOG "Instrumentation: TAU")
endforeach()
# A YAML alias gives the value of its anchor.
_salt_add_clang_error_test(config_ok_alias_include_c
  config_ok_alias_include_c.out "lang:C" hello.c EXIT_ZERO
  CHECK "-s config_ok_alias_include_c.out"
  ARGS --config_file=${_config_errors}/alias_include.yaml
  OUT "#include <Profile/Profiler.h>")
# --tau_use_cxx_api needs no main_insert, function_begin_insert, or
# function_end_insert.
_salt_add_clang_error_test(config_ok_cxx_api_cxx config_ok_cxx_api_cxx.out
  "lang:CXX" c11test.cpp EXIT_ZERO CHECK "-s config_ok_cxx_api_cxx.out"
  ARGS --config_file=${_config_errors}/cxx_api.yaml --tau_use_cxx_api
  OUT "TAU_PROFILE(")
# A selective instrumentation file that cannot be read, or a directory,
# stops saltfm too.
set(_no_such_sif ${CMAKE_SOURCE_DIR}/tests/sif/no_such_file.tau)
set(_sif_dir ${CMAKE_SOURCE_DIR}/tests/sif)
foreach(lang c cxx)
  set(_src hello.c)
  set(_label C)
  if(lang STREQUAL "cxx")
    set(_src c11test.cpp)
    set(_label CXX)
  endif()
  _salt_add_clang_error_test(clang_sif_unreadable_${lang}
    clang_sif_unreadable_${lang}.out "lang:${_label}" ${_src}
    ARGS --tau_select_file=${_no_such_sif}
    LOG "ERROR: Cannot open selective instrumentation file: ${_no_such_sif}")
  _salt_add_clang_error_test(clang_sif_dir_${lang}
    clang_sif_dir_${lang}.out "lang:${_label}" ${_src}
    ARGS --tau_select_file=${_sif_dir}
    LOG "ERROR: Cannot open selective instrumentation file: ${_sif_dir}")
endforeach()
foreach(cfg nvtx roctx)
  string(TOUPPER ${cfg} _name)
  _salt_add_clang_error_test(config_ok_${cfg}_c config_ok_${cfg}_c.out
    "lang:C" hello.c EXIT_ZERO CHECK "-s config_ok_${cfg}_c.out"
    ARGS --config_file=${CMAKE_SOURCE_DIR}/config_files/${cfg}_config.yaml
    LOG "Instrumentation: ${_name}")
endforeach()

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
  # A pass pattern hides the exit status, so a status other than 0 is
  # printed and fails the step
  add_test(NAME instrument_${name}
    COMMAND sh -c "\"$0\" \"$@\"; rc=$?
      [ $rc -eq 0 ] || echo \"saltfm exited with status $rc\"; exit $rc"
            ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
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
  _sif_set_warning_checks(instrument_${name} "${ARG_WARNINGS}"
    "Instrumentation:;saltfm exited with status")
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
# file on the file exclude list. With --tau_excluded_main_setup=file,
# main keeps TAU's setup code, and a file without main is copied.
foreach(lang c cpp)
  add_sif_test(file_exclude_${lang}
    ${lang} tests/sif_excl.${lang} tests/sif/file_exclude_${lang}.tau
    ARGS --tau_excluded_main_setup=file)
  _sif_check_file_excluded(file_exclude_${lang})
endforeach()
add_sif_test_no_output(file_exclude_nomain_c
  c tests/sif_nomain.c tests/sif/file_exclude_nomain_c.tau)
# saltfm copies a source that the file lists exclude without reading the
# configuration file, even when it parses the source for main's setup
# code.
foreach(level routine file)
  add_sif_test_no_output(bad_config_${level}_c c tests/sif_nomain.c
    tests/sif/file_exclude_nomain_c.tau
    ARGS --tau_excluded_main_setup=${level} --config_file=${_config_parse_error})
endforeach()
# An excluded empty source gets an empty copy, not a write error.
add_sif_test_no_output(file_exclude_empty_c c tests/sif_empty.c tests/sif/file_exclude_empty_c.tau)

# FILE_INCLUDE_LIST: cover both branches of the whitelist gate.
# - match: source IS on the list, so instrumentation proceeds and
#   keep_routine appears in the output.
# - nomatch: list contains a real but unrelated file, so the source under
#   test is not whitelisted and its output is a copy.
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
  add_sif_test_no_output(file_include_nomatch_${lang}
    ${lang} tests/sif_excl.${lang} tests/sif/file_include_nomatch_${lang}.tau)
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
    fortran tests/fortran/sif_excl.f90 tests/sif/file_exclude_f90.tau
    ARGS --tau_excluded_main_setup=file)
  _sif_check_file_excluded(file_exclude_fortran)

  add_sif_test(file_include_match_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_match_f90.tau)
  set_tests_properties(check_sif_file_include_match_fortran
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_fortran}"
  )
  add_sif_test_no_output(file_include_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_nomatch_f90.tau)

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
  # must NOT whitelist the file, so the output is a copy.
  add_sif_test_no_output(file_include_substring_nomatch_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_include_substring_f90.tau)

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
  # matches "sif_*.f90", so the file is excluded and the output is a
  # copy. Mirror of file_include_glob_fortran for the exclude path.
  add_sif_test_no_output(file_exclude_glob_fortran
    fortran tests/fortran/sif_excl.f90 tests/sif/file_exclude_glob_f90.tau)

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
# routine or file list removes main's timer; a file list needs the
# `file` value.
set(_sif_setup_name_c "SETUP_TIMER_NAME[(]\"int main[(]")
set(_sif_setup_name_cpp "${_sif_setup_name_c}")
set(_sif_setup_name_fortran "SETUP_TIMER_NAME[(]\"&\n     &${_sif_main_timer_fortran}")
set(_sif_file_exclude_c file_exclude_c)
set(_sif_file_exclude_cpp file_exclude_cpp)
set(_sif_file_exclude_fortran file_exclude_f90)
foreach(lang IN LISTS _sif_langs)
  foreach(sif exclude_main include_list ${_sif_file_exclude_${lang}})
    add_sif_test(setup_name_${sif}_${lang} ${lang} ${_sif_src_${lang}} tests/sif/${sif}.tau
      CONFIG tests/setup_timer_name.yaml ARGS --tau_excluded_main_setup=file)
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
# the pattern lists. The `file` value parses the source that the file
# list excludes.
string(CONCAT _sif_oneline_pass
  "\nint dropped[(]void[)] [{] return 11. [}]\n"
  ".*\n    if [(]n == 22[)] return 22.\n"
  ".* return dropped[(][)] [+] cond_dropped[(]0[)] [+] kept[(][)] - 44. [}]\n")
foreach(sif include exclude file_exclude)
  add_sif_test(oneline_${sif} c tests/sif_oneline.c tests/sif/oneline_${sif}.tau
    ARGS --tau_excluded_main_setup=file)
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
# remove quotes. The `file` value gives main its setup code in the
# source that the quoted include entry leaves out.
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
    tests/sif/file_include_quoted.tau WARNINGS "${_warning}"
    ARGS --tau_excluded_main_setup=file)
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

# saltfm gives cparse-llvm and fparse-llvm the value of
# --tau_excluded_main_setup, or else of SALT_EXCLUDED_MAIN_SETUP: none,
# routine, or file. Values ignore case, an empty one selects `routine`,
# and any other one stops saltfm with the usage (#88).
# Adds test saltfm_main_setup_<name>: `saltfm --show ARGN hello.c`, with
# SALT_EXCLUDED_MAIN_SETUP set to `value` (unset for `unset`), must
# match `pass` and must not match `fail`.
function(_salt_add_main_setup_test name value pass fail)
  set(mod "SALT_EXCLUDED_MAIN_SETUP=set:${value}")
  if(value STREQUAL "unset")
    set(mod "SALT_EXCLUDED_MAIN_SETUP=unset:")
  endif()
  add_test(NAME saltfm_main_setup_${name}
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show ${ARGN}
            ${CMAKE_SOURCE_DIR}/tests/hello.c)
  set_tests_properties(saltfm_main_setup_${name} PROPERTIES
    PASS_REGULAR_EXPRESSION "${pass}"
    FAIL_REGULAR_EXPRESSION "${fail}"
    ENVIRONMENT_MODIFICATION "${mod}"
    LABELS "lang:C;phase:smoke")
endfunction()
set(_main_setup_shown "cparse-llvm --tau_excluded_main_setup=")
# Each value, in capitals, with another value in the environment, which
# the option overrides.
foreach(main_setup none routine file)
  set(_other file)
  if(main_setup STREQUAL "file")
    set(_other none)
  endif()
  string(TOUPPER ${main_setup} _upper)
  _salt_add_main_setup_test(value_${main_setup} ${_other}
    "${_main_setup_shown}${main_setup} " "" --tau_excluded_main_setup=${_upper})
endforeach()
# The values of the old option name are not values of this option.
foreach(main_setup never function always on off)
  _salt_add_main_setup_test(old_${main_setup} unset
    "ERROR: unknown value '${main_setup}' for --tau_excluded_main_setup\n.*USAGE:"
    "${_main_setup_shown}" --tau_excluded_main_setup=${main_setup})
endforeach()
_salt_add_main_setup_test(env File "${_main_setup_shown}file " "")
_salt_add_main_setup_test(env_empty "" "${_main_setup_shown}routine " "")
_salt_add_main_setup_test(default unset "${_main_setup_shown}routine " "")
_salt_add_main_setup_test(option_empty none "${_main_setup_shown}routine " ""
  --tau_excluded_main_setup=)
# The value can also be the next argument. Without one, the option at the
# end gives a warning and is left out.
_salt_add_main_setup_test(option_next none "${_main_setup_shown}file " ""
  --tau_excluded_main_setup File)
string(CONCAT _main_setup_no_value
  "WARNING: --tau_excluded_main_setup needs a value. using\n"
  "  SALT_EXCLUDED_MAIN_SETUP=file\n.*${_main_setup_shown}file [^\n]*/hello[.]c\n")
add_test(NAME saltfm_main_setup_no_value
  COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
          ${CMAKE_SOURCE_DIR}/tests/hello.c --tau_excluded_main_setup)
set_tests_properties(saltfm_main_setup_no_value PROPERTIES
  PASS_REGULAR_EXPRESSION "${_main_setup_no_value}"
  ENVIRONMENT_MODIFICATION "SALT_EXCLUDED_MAIN_SETUP=set:file"
  LABELS "lang:C;phase:smoke")
_salt_add_main_setup_test(unknown none
  "ERROR: unknown value 'sometimes' for --tau_excluded_main_setup\n.*USAGE:"
  "${_main_setup_shown}" --tau_excluded_main_setup=sometimes)
_salt_add_main_setup_test(unknown_env always
  "ERROR: unknown value 'always' for SALT_EXCLUDED_MAIN_SETUP\n.*USAGE:"
  "${_main_setup_shown}")
# saltfm checks the Fortran-only SALT_PARSE_EXCLUDED_MODULES only for Fortran.
_salt_add_main_setup_test(modules_env_c unset "${_main_setup_shown}routine " "ERROR")
set_property(TEST saltfm_main_setup_modules_env_c APPEND
  PROPERTY ENVIRONMENT_MODIFICATION "SALT_PARSE_EXCLUDED_MODULES=set:bogus")
# cparse-llvm takes only the canonical names.
add_test(NAME cparse_main_setup_canonical_only
  COMMAND $<TARGET_FILE:cparse-llvm> --tau_excluded_main_setup=File
          ${CMAKE_SOURCE_DIR}/tests/hello.c --)
set_tests_properties(cparse_main_setup_canonical_only PROPERTIES
  PASS_REGULAR_EXPRESSION "Cannot find option named 'File'"
  LABELS "lang:C;phase:smoke")
# saltfm takes the language from a source, not from an option value
# such as an output file name, also when the value is the next
# argument, which saltfm passes on after the option, or the option has
# one dash, as LLVM's option parser allows.
set(_option_langs c)
if(TEST_FORTRAN)
  list(APPEND _option_langs fortran)
endif()
foreach(lang IN LISTS _option_langs)
  set(_src ${CMAKE_SOURCE_DIR}/tests/hello.c)
  set(_out out.inst.f08)
  set(_tool cparse-llvm)
  set(_label C)
  if(lang STREQUAL "fortran")
    set(_src ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set(_out out.inst.c)
    set(_tool fparse-llvm)
    set(_label Fortran)
  endif()
  string(REPLACE "." "[.]" _out_re "${_out}")
  foreach(dash IN ITEMS -- -)
    set(_name ${lang})
    if(dash STREQUAL "-")
      set(_name dash_${lang})
    endif()
    add_test(NAME saltfm_option_value_${_name}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              ${dash}tau_output=${_out} ${_src})
    set_tests_properties(saltfm_option_value_${_name} PROPERTIES
      PASS_REGULAR_EXPRESSION "^${_tool} "
      LABELS "lang:${_label};phase:smoke")
    foreach(opt config_file flang_exe tau_output tau_select_file)
      add_test(NAME saltfm_option_next_${opt}_${_name}
        COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
                ${dash}${opt} ${_out} ${_src})
      set_tests_properties(saltfm_option_next_${opt}_${_name} PROPERTIES
        PASS_REGULAR_EXPRESSION "^${_tool} [^\n]* ${dash}${opt} ${_out_re} "
        LABELS "lang:${_label};phase:smoke")
    endforeach()
  endforeach()
  foreach(dash IN ITEMS - --)
    set(_name o_${lang})
    if(dash STREQUAL "--")
      set(_name dashes_o_${lang})
    endif()
    add_test(NAME saltfm_option_next_${_name}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              ${dash}o ${_out} ${_src})
    set_tests_properties(saltfm_option_next_${_name} PROPERTIES
      PASS_REGULAR_EXPRESSION "^${_tool} [^\n]* ${dash}o ${_out_re} "
      LABELS "lang:${_label};phase:smoke")
  endforeach()
endforeach()
if(TEST_FORTRAN)
  # saltfm gives fparse-llvm the canonical value, which fparse-llvm gives
  # the plugin; fparse-llvm takes only the canonical names.
  add_test(NAME saltfm_main_setup_fortran
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            --tau_excluded_main_setup=None
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  add_test(NAME fparse_main_setup_canonical_only
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_excluded_main_setup=None
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(saltfm_main_setup_fortran PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_EXCLUDED_MAIN_SETUP=none\ncmd: ")
  set_tests_properties(fparse_main_setup_canonical_only PROPERTIES
    PASS_REGULAR_EXPRESSION "ERROR: unknown value 'None' for --tau_excluded_main_setup"
    FAIL_REGULAR_EXPRESSION "cmd: ")
  # Both also take the value as the next argument.
  add_test(NAME saltfm_main_setup_fortran_next
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            --tau_excluded_main_setup None
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  add_test(NAME fparse_main_setup_next
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_excluded_main_setup file
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(saltfm_main_setup_fortran_next PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_EXCLUDED_MAIN_SETUP=none\ncmd: ")
  set_tests_properties(fparse_main_setup_next PROPERTIES
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_EXCLUDED_MAIN_SETUP=file\ncmd: ")
  add_test(NAME fparse_main_setup_no_value
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 --tau_excluded_main_setup)
  string(CONCAT _main_setup_no_value
    "WARNING: --tau_excluded_main_setup needs a value. using routine\n"
    ".*\nSALT_FORTRAN_EXCLUDED_MAIN_SETUP=routine\ncmd: ")
  set_tests_properties(fparse_main_setup_no_value PROPERTIES
    PASS_REGULAR_EXPRESSION "${_main_setup_no_value}")
  # saltfm and fparse-llvm read their options only from their arguments,
  # not from environment variables named like their shell variables.
  set(_shell_var_tests "")
  string(CONCAT _shell_var_shown "^fparse-llvm --show "
    "--tau_parse_excluded_modules=modules-only --tau_excluded_main_setup=routine ")
  foreach(var excluded_main_setup=none parse_excluded_modules=off
      tool=cparse-llvm version=true build_uses_salt_flang=true)
    string(REGEX REPLACE "=.*" "" name "${var}")
    add_test(NAME saltfm_shell_var_${name}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set_tests_properties(saltfm_shell_var_${name} PROPERTIES
      ENVIRONMENT "${var}"
      PASS_REGULAR_EXPRESSION "${_shell_var_shown}")
    list(APPEND _shell_var_tests saltfm_shell_var_${name})
  endforeach()
  # Without --show, saltfm runs the instrumentor.
  add_test(NAME saltfm_shell_var_show
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
            --tau_output=shell_var_show.inst.F90
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(saltfm_shell_var_show PROPERTIES
    ENVIRONMENT "show=true;SALT_FORTRAN_VERBOSE=1"
    PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
  _salt_in_test_output(instrument saltfm_shell_var_show)
  # Without --lang, --help gives the saltfm usage.
  add_test(NAME saltfm_shell_var_force_lang
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --help)
  set_tests_properties(saltfm_shell_var_force_lang PROPERTIES
    ENVIRONMENT "force_lang=c"
    PASS_REGULAR_EXPRESSION "USAGE: [^\n]*/saltfm \\[options\\]")
  # Without an input file argument, fparse-llvm names none.
  add_test(NAME fparse_shell_var_input_file
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show)
  set_tests_properties(fparse_shell_var_input_file PROPERTIES
    ENVIRONMENT "input_file=shell_var.f90"
    PASS_REGULAR_EXPRESSION "(^|\n)input file: \"<None given>\" \n"
    FAIL_REGULAR_EXPRESSION "unbound variable")
  # Without --config_file, fparse-llvm reads the default file.
  add_test(NAME fparse_shell_var_config_file
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  string(CONCAT _shell_var_config "\nSALT_FORTRAN_CONFIG_FILE=\"[^\n]*/"
    "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}/config_files/config\\.yaml\"\n")
  set_tests_properties(fparse_shell_var_config_file PROPERTIES
    ENVIRONMENT "FORTRAN_CONFIG_FILE=/env/leak.yaml"
    PASS_REGULAR_EXPRESSION "${_shell_var_config}")
  # Without --show, fparse-llvm stops with an error and does not run Flang,
  # also when it has a flag, which is not an input file (#102).
  foreach(name IN ITEMS no_input no_input_flag)
    set(_arg --tau_excluded_main_setup=routine)
    if(name STREQUAL "no_input_flag")
      set(_arg -fopenmp)
    endif()
    add_test(NAME fparse_${name}
      COMMAND sh -c "\"$0\" \"$@\" > fparse_${name}.log 2>&1; rc=$?
        cat fparse_${name}.log
        [ $rc -ge 1 ] && [ $rc -le 127 ] ||
          { echo \"Expected an exit status from 1 to 127, got $rc\"; exit 1; }
        grep -qF 'ERROR: no input file' fparse_${name}.log ||
          { echo 'Expected ERROR: no input file'; exit 1; }
        ! grep -qE 'Running:|unbound variable' fparse_${name}.log ||
          { echo 'Did not expect Running: or unbound variable'; exit 1; }"
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm ${_arg})
    _salt_in_test_output(instrument fparse_${name})
  endforeach()
  # fparse-llvm takes a Fortran source after a flag as the input file,
  # and without one, the first argument that is not a flag (#102).
  add_test(NAME fparse_input_after_flag
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            -fopenmp foo.f08)
  string(CONCAT _input_after_flag "(^|\n)input file: foo\\.f08\n.*\n"
    "cmd: [^\n]* foo\\.f08 -o [^\n]*/foo\\.inst\\.F90 -fopenmp\n")
  set_tests_properties(fparse_input_after_flag PROPERTIES
    PASS_REGULAR_EXPRESSION "${_input_after_flag}")
  add_test(NAME fparse_input_not_flag
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            -fopenmp foo.inc)
  set_tests_properties(fparse_input_not_flag PROPERTIES
    PASS_REGULAR_EXPRESSION "(^|\n)input file: foo\\.inc\n")
  # A flag with a Fortran extension, such as a macro definition, is not
  # the input file; fparse-llvm forwards it to Flang.
  foreach(tool fparse-llvm saltfm)
    string(REPLACE "-llvm" "" _name ${tool}_input_not_macro)
    add_test(NAME ${_name}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/${tool} --show
              hello.f90 -- -DSRC=util.f08)
    set_tests_properties(${_name} PROPERTIES
      PASS_REGULAR_EXPRESSION
        "(^|\n)input file: hello\\.f90\n.*\ncmd: [^\n]* hello\\.f90 -o [^\n]* -DSRC=util\\.f08\n")
    list(APPEND _shell_var_tests ${_name})
  endforeach()
  # fparse-llvm reports each argument that is not a flag and that it does
  # not take as the input file.
  add_test(NAME fparse_unused_args
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            first.inc foo.f08 second.inc)
  string(CONCAT _unused_args "(^|\n)Removed unknown flag: first\\.inc\n"
    "Removed unknown flag: second\\.inc\ninput file: foo\\.f08\n")
  set_tests_properties(fparse_unused_args PROPERTIES
    PASS_REGULAR_EXPRESSION "${_unused_args}")
  add_test(NAME fparse_unused_args_not_flag
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            first.inc second.inc)
  set_tests_properties(fparse_unused_args_not_flag PROPERTIES
    PASS_REGULAR_EXPRESSION
      "(^|\n)Removed unknown flag: second\\.inc\ninput file: first\\.inc\n"
    FAIL_REGULAR_EXPRESSION "Removed unknown flag: first")
  # saltfm gives fparse-llvm a source with each extension that Flang or
  # gfortran reads as Fortran, and fparse-llvm takes it as the input
  # file, not the argument before it (#102).
  set(_ext_tests "")
  foreach(ext f F for FOR ftn FTN f77 fpp FPP f90 F90 f95 F95 f03 F03
      f08 F08 f18 F18)
    add_test(NAME saltfm_ext_${ext}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              other x.${ext})
    set_tests_properties(saltfm_ext_${ext} PROPERTIES
      PASS_REGULAR_EXPRESSION "\ninput file: x\\.${ext}\n")
    list(APPEND _ext_tests saltfm_ext_${ext})
  endforeach()
  # Without --tau_output, fparse-llvm must neither remove nor write the
  # file that an exported output_file names.
  add_test(NAME fparse_shell_var_output_file
    COMMAND sh -c "mkdir -p shell_var && cd shell_var || exit 1
      rm -f keep.txt hello.inst.F90 && echo keep > keep.txt || exit 1
      output_file=keep.txt \"$0\" \"$@\" || exit 1
      grep -qx keep keep.txt || { echo 'keep.txt was removed or changed'; exit 1; }
      [ -s hello.inst.F90 ] || { echo 'Expected hello.inst.F90'; exit 1; }"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm
      ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  _salt_in_test_output(instrument fparse_shell_var_output_file)
  add_test(NAME fparse_shell_var_select_file
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(fparse_shell_var_select_file PROPERTIES
    ENVIRONMENT "select_file=shell_var.tau"
    PASS_REGULAR_EXPRESSION "\nSALT_FORTRAN_SELECT_FILE=\"\"\n")
  add_test(NAME fparse_shell_var_build_uses_salt_flang
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(fparse_shell_var_build_uses_salt_flang PROPERTIES
    ENVIRONMENT "build_uses_salt_flang=true"
    PASS_REGULAR_EXPRESSION "\ncmd: [^\n]* -module-suffix ")
  list(APPEND _shell_var_tests saltfm_shell_var_show saltfm_shell_var_force_lang
    fparse_shell_var_input_file fparse_shell_var_config_file
    fparse_shell_var_output_file fparse_shell_var_select_file
    fparse_shell_var_build_uses_salt_flang fparse_no_input
    fparse_no_input_flag fparse_input_after_flag fparse_input_not_flag
    fparse_unused_args fparse_unused_args_not_flag ${_ext_tests})
  set_tests_properties(saltfm_main_setup_fortran fparse_main_setup_canonical_only
    saltfm_main_setup_fortran_next fparse_main_setup_next fparse_main_setup_no_value
    ${_shell_var_tests}
    PROPERTIES
    LABELS "lang:Fortran;phase:smoke")
endif()

# Unless --tau_excluded_main_setup=file, saltfm does not parse a source
# that the file lists exclude: a Clang error in it does not stop saltfm,
# and saltfm copies it. With `file`, the error does.
foreach(level none routine)
  add_sif_test_no_output(parse_error_${level}_c c tests/clang_error.c
    tests/sif/file_exclude_all.tau ARGS --tau_excluded_main_setup=${level})
endforeach()
_salt_add_clang_error_test(parse_error_file_c parse_error_file.inst.c
  "lang:C" clang_error.c ARGS --tau_excluded_main_setup=file
  --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau
  LOG "undeclared identifier 'undeclared_name'")
# The `#line` lines and the timer names of C and C++ output escape `\`
# and `"`, of the file name and of the signature; a `${full_timer_name}`
# in the file name stays as written. Adds test
# line_escape_<name>: tests/check_line_escape.cmake on `src` (relative
# to tests/) as a .`ext` file, compiled and run with
# tests/line_escape.yaml, or with TAU, checked as text with the default
# configuration file. With EXCLUDED, the file lists exclude it. ARGN are
# more -D options of the script.
function(_salt_add_line_escape_test name ext src)
  cmake_parse_arguments(PARSE_ARGV 3 ARG "TAU;EXCLUDED" "" "")
  set(compiler ${CMAKE_C_COMPILER})
  if(ext STREQUAL "cpp")
    set(compiler ${CMAKE_CXX_COMPILER})
  endif()
  set(args -DCOMPILER=${compiler}
    -DCONFIG=${CMAKE_SOURCE_DIR}/tests/line_escape.yaml)
  if(ARG_TAU)
    set(args "")
  endif()
  if(ARG_EXCLUDED)
    list(APPEND args -DSIF=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau)
  elseif(LLVM_VERSION_MAJOR LESS 23)
    # Clang's tooling before LLVM 23 turns each `\` of a source path
    # into `/`, so it cannot open the source
    list(APPEND args -DNO_BACKSLASH=ON)
  endif()
  add_test(NAME line_escape_${name}
    COMMAND ${CMAKE_COMMAND}
      -DSALTFM=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
      -DSRC=${CMAKE_SOURCE_DIR}/tests/${src} -DEXT=${ext}
      -DNAME=line_escape_${name} ${args} ${ARG_UNPARSED_ARGUMENTS}
      -P ${CMAKE_SOURCE_DIR}/tests/check_line_escape.cmake)
  _salt_lang_to_label(${ext} lang_label)
  set_tests_properties(line_escape_${name} PROPERTIES
    LABELS "${lang_label};phase:run")
  _salt_in_test_output(instrument line_escape_${name})
endfunction()
foreach(ext c cpp)
  _salt_add_line_escape_test(instrumented_${ext} ${ext} line_escape.c)
  _salt_add_line_escape_test(excluded_${ext} ${ext} line_escape.c EXCLUDED)
  _salt_add_line_escape_test(tau_${ext} ${ext} line_escape.c TAU)
endforeach()
# A `"` or `\` in the signature, as Clang prints it: a C++ user-defined
# literal operator, char template arguments, and a C++20 string literal
# template argument
_salt_add_line_escape_test(udl_cpp cpp line_escape_udl.cpp
  [[-DEXPECT=operator""_km(]])
_salt_add_line_escape_test(chars_cpp cpp line_escape_chars.cpp
  [[-DEXPECT=int f(S<'"'>, S<'\\'>, S<'\n'>) ]])
_salt_add_line_escape_test(string_cpp cpp line_escape_string.cpp
  -DSTD=c++20 [[-DEXPECT=int g(T<"a\"b">) ]])
if(TEST_FORTRAN)
  foreach(level none routine)
    add_sif_test_no_output(parse_error_${level}_fortran fortran
      tests/fortran/compile_error.f90 tests/sif/file_exclude_all.tau
      ARGS --tau_excluded_main_setup=${level})
  endforeach()
  _salt_add_clang_error_test(parse_error_file_fortran
    parse_error_file.inst.F90 "lang:Fortran" fortran/compile_error.f90
    ARGS --tau_excluded_main_setup=file
    --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau
    LOG "undeclared_name")
endif()

# A `#line` line in the input moves no TAU call off its line of the
# input, and a Fortran timer name gives the file and lines that the
# `#line` lines give, as compiler messages do (#112). Adds test
# line_directive_<name>: saltfm, run in test-output/instrument (for
# module files) on `src` in tests/line_directive with its config.yaml,
# must write the output `expected`, which leaves out the `#line` lines
# that saltfm writes (with no file, or the path of `src`), the blank
# lines, and the path of that directory, but keeps those of `src`.
# Case is ignored, as Flang versions differ in the case of a program
# name.
function(_salt_add_line_directive_test name src expected label)
  set(dir ${CMAKE_SOURCE_DIR}/tests/line_directive)
  set(out ${SALT_TEST_OUTPUT_DIR}/instrument/line_directive_${name}.out)
  add_test(NAME line_directive_${name}
    COMMAND sh -c "\"$0\" \"$@\" || exit 1
      grep -Ev -e '^#line [0-9]+( \"${dir}/${src}\")?$' -e '^[[:space:]]*$' ${out} |
        sed 's|${dir}/||g' | diff -iu ${dir}/${expected} -"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
      --config_file=${dir}/config.yaml --tau_output=${out} ${dir}/${src})
  set_tests_properties(line_directive_${name} PROPERTIES
    REQUIRED_FILES ${dir}/${src}
    LABELS "${label};phase:instrument")
  _salt_in_test_output(instrument line_directive_${name})
endfunction()
_salt_add_line_directive_test(c mid.c mid.c.expected "lang:C")
_salt_add_line_directive_test(cxx mid.cpp mid.cpp.expected "lang:CXX")
if(TEST_FORTRAN)
  _salt_add_line_directive_test(top top.f90 top.f90.expected "lang:Fortran")
  _salt_add_line_directive_test(top_hash top_hash.f90 top_hash.f90.expected
    "lang:Fortran")
  _salt_add_line_directive_test(mid mid.f90 mid.f90.expected "lang:Fortran")
  # A separate module procedure and a function
  _salt_add_line_directive_test(procs procs.f90 procs.f90.expected
    "lang:Fortran")
  # Fixed form
  _salt_add_line_directive_test(fixed fixed.f fixed.f.expected "lang:Fortran")
  # A `#line` line that changes only the file
  _salt_add_line_directive_test(implicit implicit.f90 implicit.f90.expected
    "lang:Fortran")
  # An output of saltfm as the input
  _salt_add_line_directive_test(instrumented hello.inst.F90
    hello.inst.F90.expected "lang:Fortran")
  # The return statement of this included file puts the instrumentation
  # points out of line order, so saltfm must stop with an error.
  _salt_add_clang_error_test(line_directive_include_return
    line_directive_include_return.out "lang:Fortran"
    line_directive/include_return.f90
    ARGS --config_file=${CMAKE_SOURCE_DIR}/tests/line_directive/config.yaml
    LOG "ERROR: Instrumentation points of ${CMAKE_SOURCE_DIR}/tests/line_directive/include_return"
    "include_return.f90 are not in line order, as when an included file holds instrumented code")
endif()

# Unless --tau_excluded_main_setup=file, the main program of a source
# that the file lists exclude gets no setup code, and saltfm warns about
# it.
set(_excluded_main_warning
  "WARNING: [^\n]*/sif_excl[.](c|cpp|f90) seems to contain the main program")
foreach(lang IN LISTS _sif_langs)
  foreach(level none routine)
    add_sif_test_no_output(file_exclude_main_${level}_${lang} ${lang}
      ${_sif_src_${lang}} tests/sif/${_sif_file_exclude_${lang}}.tau
      ARGS --tau_excluded_main_setup=${level})
    set_tests_properties(instrument_sif_file_exclude_main_${level}_${lang}
      PROPERTIES PASS_REGULAR_EXPRESSION "${_excluded_main_warning}")
  endforeach()
  set_property(TEST warn_sif_file_exclude_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "${_excluded_main_warning}")
endforeach()
set_property(TEST instrument_sif_file_exclude_nomain_c
  APPEND PROPERTY FAIL_REGULAR_EXPRESSION "seems to contain the main program")
# A `main(` in a comment or a string literal is not a main program.
foreach(lang c cpp)
  add_sif_test_no_output(file_exclude_main_text_${lang} ${lang}
    tests/sif_main_text.${lang} tests/sif/file_exclude_all.tau)
  set_property(TEST instrument_sif_file_exclude_main_text_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "seems to contain the main program")
endforeach()

# With `none`, a main program that the routine lists leave out gets no
# setup code, and saltfm does not warn about the include list; with
# `file`, it gets the setup code, as with `routine`.
foreach(lang IN LISTS _sif_langs)
  foreach(level none file)
    add_sif_test(main_excluded_${level}_${lang} ${lang} ${_sif_src_${lang}}
      tests/sif/exclude_main.tau ARGS --tau_excluded_main_setup=${level})
    set_tests_properties(check_sif_main_excluded_${level}_${lang}
      PROPERTIES FAIL_REGULAR_EXPRESSION "${_sif_main_timer_${lang}}")
  endforeach()
  set_property(TEST check_sif_main_excluded_none_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  set_tests_properties(check_sif_main_excluded_file_${lang}
    PROPERTIES PASS_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  add_sif_test(include_list_none_${lang} ${lang} ${_sif_src_${lang}}
    tests/sif/include_list.tau ARGS --tau_excluded_main_setup=none)
  set_tests_properties(check_sif_include_list_none_${lang}
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${_sif_keep_pattern_${lang}}"
    FAIL_REGULAR_EXPRESSION "TAU_PROFILE_SET_NODE")
  set_property(TEST warn_sif_include_list_none_${lang}
    APPEND PROPERTY FAIL_REGULAR_EXPRESSION "${_sif_main_warning}")
endforeach()

# TAU's selective instrumentation reference cases (tests/sif/tau/):
# sif_check reads each case's SIF with SALT-FM's parser and checks the
# routine and file names of its .expect file with SALT-FM's matcher.
add_executable(sif_check tests/sif_check.cpp)
target_link_libraries(sif_check PRIVATE salt_selectfile_obj
  salt_output_file_obj ${SALT_LLVM_SUPPORT_LIBS})
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
    WORKING_DIRECTORY ${_fx_bin}
    ENVIRONMENT "PATH=${_fx_dir}/nodriver")
  # Without salt-selectfile beside it, fparse-llvm warns and runs Flang.
  _salt_configure_fparse_llvm(${_fx_dir}/noselect/fparse-llvm
    ${_fx_dir}/match/flang)
  _salt_add_flang_exe_test(fparse_selectfile_missing
    "WARNING: salt-selectfile failed with status 127.*stub driver must not run" "ERROR"
    ${_fx_dir}/noselect/fparse-llvm --tau_output=noselect.inst.F90
    --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau ${_fx_src})

  # A driver configure did not find is not recorded: no warning.
  _salt_add_flang_exe_test(fparse_flang_exe_no_record
    "${_fx_match_cmd}" "WARNING: recorded"
    ${_fx_bin}/fparse-llvm-no-record --show ${_fx_src})
  set_property(TEST fparse_flang_exe_no_record APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_fx_dir}/match")

  # With --tau_build_uses_salt_flang, -fc1 gets the intrinsic module
  # directory that the driver prints with `-###`, but not one with an
  # escaped character.
  _salt_add_flang_exe_test(fparse_flang_exe_probe_mods
    "${_fx_match_cmd}[^\n]* -fintrinsic-modules-path /stub/finclude "
    "${_fx_stub_ran};WARNING"
    ${_fx_bin}/fparse-llvm --show --tau_build_uses_salt_flang
    --flang_exe=${_fx_dir}/match/flang ${_fx_src})
  set_property(TEST fparse_flang_exe_probe_mods APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "STUB_INTRINSIC_MODS=set:/stub/finclude")
  _salt_add_flang_exe_test(fparse_flang_exe_probe_mods_escaped
    "WARNING: cannot read the intrinsic module directory"
    "${_fx_stub_ran};-fintrinsic-modules-path"
    ${_fx_bin}/fparse-llvm --show --tau_build_uses_salt_flang
    --flang_exe=${_fx_dir}/match/flang ${_fx_src})
  set_property(TEST fparse_flang_exe_probe_mods_escaped APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "STUB_INTRINSIC_MODS=set:/stub/a\\\"b")

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
    "Remaining Arguments: ${_fx_flags}\n.*"
    "${_fx_match_cmd}[^\n]* ${_fx_flags}\n")
  _salt_add_flang_exe_test(fparse_whole_flag_names
    "${_fx_pass}" "cmd:[^\n]*decoy-"
    ${_fx_bin}/fparse-llvm --show --flang_exe=${_fx_dir}/match/flang
    -fopenmp -Werror -Wall -Ifoo -Iinc-Wave -DBAR=1 -Jdir -std=f2018
    -Wl,-rpath,x fortran_decoy-Stuff.inc fortran_decoy-Wstuff.inc ${_fx_src})
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
  # With BUILD_USES_SALT_FLANG, the directory has only the .mod file that
  # the Flang driver writes, and saltfm gets --tau_build_uses_salt_flang.
  function(_salt_add_user_mod_test src flag)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "BUILD_USES_SALT_FLANG" "" "")
    get_filename_component(base ${src} NAME_WLE)
    set(dir ${_user_mod_dir})
    set(fixture user_iso_fortran_env_mod)
    set(option "")
    if(ARG_BUILD_USES_SALT_FLANG)
      string(APPEND base _build_uses_salt_flang)
      set(dir ${_user_mod_flang_dir})
      set(fixture user_iso_fortran_env_flang_mod)
      set(option --tau_build_uses_salt_flang)
    endif()
    set(name instrument_${base}${flag})
    add_test(NAME ${name}
      COMMAND
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm ${option}
        ${flag}${dir} --tau_output=${base}${flag}.inst.F90
        ${CMAKE_SOURCE_DIR}/tests/fortran/${src})
    set_tests_properties(${name} PROPERTIES
      FIXTURES_REQUIRED ${fixture}
      REQUIRED_FILES "${_required_files}"
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
    _salt_in_test_output(instrument ${name})
  endfunction()
  _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -I)
  _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -J)
  _salt_add_user_mod_test(use-mixed-nature.f90 -I)
  # So must the compiler's own modules that --tau_build_uses_salt_flang
  # reads (#40). The Flang driver writes the user module, as a build does.
  if(_salt_test_flang)
    set(_user_mod_flang_dir ${SALT_TEST_OUTPUT_DIR}/user-mod-flang)
    add_test(NAME make_user_iso_fortran_env_flang_mod
      COMMAND sh -c "rm -rf \"$0\" && mkdir -p \"$0\" && cd \"$0\" &&
        \"$1\" -c -J. \"$2\""
        ${_user_mod_flang_dir} ${_salt_test_flang}
        ${CMAKE_SOURCE_DIR}/tests/fortran/user-iso-fortran-env.f90)
    set_tests_properties(make_user_iso_fortran_env_flang_mod PROPERTIES
      FIXTURES_SETUP user_iso_fortran_env_flang_mod
      LABELS "lang:Fortran;phase:setup")
    _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -I BUILD_USES_SALT_FLANG)
    _salt_add_user_mod_test(use-user-iso-fortran-env.f90 -J BUILD_USES_SALT_FLANG)
    _salt_add_user_mod_test(use-mixed-nature.f90 -I BUILD_USES_SALT_FLANG)
  endif()

  # Unless --tau_excluded_main_setup=file, fparse-llvm parses a Fortran
  # source that the file lists exclude only when it seems to define a
  # module (or, with submodules-and-modules, a submodule), or holds an
  # INCLUDE line or an `#include`, so that Flang writes the module files,
  # and the plugin copies it (#88).
  # These are the texts of its warnings.
  set(_excluded_dir ${SALT_TEST_OUTPUT_DIR}/excluded)
  set(_excluded_main_warning_text "seems to contain the main program")
  set(_excluded_parse_text "for its module files")
  set(_excluded_flang_error_text "WARNING: Flang could not parse")
  set(_excluded_off_text "WARNING: not parsing")
  # Adds test excluded_<name>: saltfm, run in a fresh
  # test-output/excluded/<name> directory with -J., the SIF `SIF`
  # (default: one that excludes all files), the ARGS, and `src` (relative
  # to tests/fortran, or absolute), must exit with status 0 and write
  # `src` as a copy. With MOD, it must write `MOD`.saltmod; without, no
  # .saltmod file. Each LOG text must be in its output, and no NOT_LOG
  # text.
  function(_salt_add_excluded_test name src)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "MOD;SIF" "ARGS;LOG;NOT_LOG")
    set(sif tests/sif/file_exclude_all.tau)
    if(ARG_SIF)
      set(sif ${ARG_SIF})
    endif()
    cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/fortran)
    set(dir ${_excluded_dir}/${name})
    set(check_copy ${CMAKE_SOURCE_DIR}/tests/check_copy.cmake)
    set(mod_check "! ls *.saltmod > /dev/null 2>&1")
    if(ARG_MOD)
      set(mod_check "[ -f ${ARG_MOD}.saltmod ]")
    endif()
    set(text_checks "")
    foreach(text IN LISTS ARG_LOG)
      string(APPEND text_checks "grep -qF -- \"${text}\" log ||
        { echo \"Expected ${text}\"; exit 1; }\n")
    endforeach()
    foreach(text IN LISTS ARG_NOT_LOG)
      string(APPEND text_checks "! grep -qF -- \"${text}\" log ||
        { echo \"Did not expect ${text}\"; exit 1; }\n")
    endforeach()
    add_test(NAME excluded_${name}
      COMMAND sh -c "rm -rf ${dir} && mkdir -p ${dir} && cd ${dir} || exit 1
        \"$0\" \"$@\" > log 2>&1; rc=$?
        cat log
        [ $rc -eq 0 ] || { echo \"Expected an exit status of 0, got $rc\"; exit 1; }
        ${mod_check} || { echo \"Expected ${mod_check}\"; exit 1; }
        ${CMAKE_COMMAND} -DSRC=${src} -DOUT=out.inst.F90 -P ${check_copy} || exit 1
        ${text_checks}"
        ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm -J. ${ARG_ARGS}
        --tau_select_file=${CMAKE_SOURCE_DIR}/${sif}
        --tau_output=out.inst.F90 ${src})
    set_tests_properties(excluded_${name} PROPERTIES
      REQUIRED_FILES "${src}"
      LABELS "lang:Fortran;phase:instrument")
  endfunction()
  # The module files of an excluded module source let another source use
  # the module; only `none` warns about the parse. So do those
  # of a source whose module comes from an INCLUDE line or an `#include`.
  set(_excluded_mod_none excluded_mod.f90
    ARGS --tau_excluded_main_setup=none LOG "${_excluded_parse_text}")
  set(_excluded_mod_routine excluded_mod.f90
    ARGS --tau_excluded_main_setup=routine NOT_LOG "${_excluded_parse_text}")
  set(_excluded_mod_include excluded_mod_include.f90)
  set(_excluded_mod_cpp_include excluded_mod_cpp_include.F90)
  foreach(case none routine include cpp_include)
    _salt_add_excluded_test(mod_${case} ${_excluded_mod_${case}}
      MOD excluded_mod)
    set_tests_properties(excluded_mod_${case} PROPERTIES
      FIXTURES_SETUP excluded_mod_${case})
    add_test(NAME instrument_use_excluded_mod_${case}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
              -I${_excluded_dir}/mod_${case}
              --tau_output=use-excluded-mod_${case}.inst.F90
              ${CMAKE_SOURCE_DIR}/tests/fortran/use-excluded-mod.f90)
    set_tests_properties(instrument_use_excluded_mod_${case} PROPERTIES
      FIXTURES_REQUIRED excluded_mod_${case}
      ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
      LABELS "lang:Fortran;phase:instrument"
      PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
    _salt_in_test_output(instrument instrument_use_excluded_mod_${case})
  endforeach()
  # Only its child submodules need the module file of a submodule, so by
  # default an excluded source with a submodule and no module is copied
  # without a parse or a warning; with submodules-and-modules, Flang
  # writes <module>-<submodule>.saltmod, which a child submodule reads.
  _salt_add_excluded_test(smod_parent excluded_smod_parent.f90 MOD smod_m)
  _salt_add_excluded_test(smod_default excluded_smod.f90
    NOT_LOG "Running:" "WARNING:")
  # A source with a module and a submodule is a module source.
  _salt_add_excluded_test(mod_smod excluded_mod_smod.f90 MOD mod_smod_m
    LOG "Running:")
  _salt_add_excluded_test(smod_parse excluded_smod.f90 MOD smod_m-smod_s
    ARGS -I${_excluded_dir}/smod_parent
    --tau_parse_excluded_modules=submodules-and-modules
    LOG "Running:" NOT_LOG "WARNING:")
  # Without its module files, only the child submodules can fail, at
  # `off` and when Flang cannot parse it (here, without the parent's).
  set(_smod_fail_text "child submodules of its submodules can fail")
  _salt_add_excluded_test(smod_off excluded_smod.f90
    ARGS --tau_parse_excluded_modules=off
    LOG "${_excluded_off_text}" "${_smod_fail_text}"
    NOT_LOG "Running:" "sources that use")
  _salt_add_excluded_test(smod_parse_error excluded_smod.f90
    ARGS --tau_parse_excluded_modules=submodules-and-modules
    LOG "${_excluded_flang_error_text}" "${_smod_fail_text}"
    NOT_LOG "sources that use")
  set_tests_properties(excluded_smod_parent PROPERTIES
    FIXTURES_SETUP excluded_smod_parent)
  set_tests_properties(excluded_smod_parse PROPERTIES
    FIXTURES_REQUIRED excluded_smod_parent
    FIXTURES_SETUP excluded_smod_parse)
  add_test(NAME instrument_use_excluded_smod
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
            -I${_excluded_dir}/smod_parent -I${_excluded_dir}/smod_parse
            --tau_output=use-excluded-smod.inst.F90
            ${CMAKE_SOURCE_DIR}/tests/fortran/use-excluded-smod.f90)
  set_tests_properties(instrument_use_excluded_smod PROPERTIES
    FIXTURES_REQUIRED "excluded_smod_parent;excluded_smod_parse"
    ENVIRONMENT "SALT_FORTRAN_VERBOSE=1"
    LABELS "lang:Fortran;phase:instrument"
    PASS_REGULAR_EXPRESSION "SALT Instrumentor Plugin finished")
  _salt_in_test_output(instrument instrument_use_excluded_smod)
  # When Flang cannot parse the module source, saltfm warns and copies it.
  _salt_add_excluded_test(mod_error excluded_mod_error.f90
    LOG "${_excluded_flang_error_text}")
  # With only an INCLUDE line, the warning does not say it has modules.
  _salt_add_excluded_test(include_error excluded_include_error.f90
    LOG "${_excluded_flang_error_text}"
    "write any module files it defines, so sources that use them can fail"
    NOT_LOG "its module")
  # It also gives the SIF warnings, which the plugin gives when Flang can.
  _salt_add_excluded_test(mod_error_sif_warning excluded_mod_error.f90
    SIF tests/sif/file_include_quoted.tau
    LOG "file include list entry at selective instrumentation file line 4")
  # A missing excluded source gives an error, and no output.
  _salt_add_clang_error_test(excluded_missing_input
    excluded_missing_input.inst.F90 "lang:Fortran" fortran/no_such_source.f90
    ARGS --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau
    LOG "ERROR: Could not open input file")
  set_tests_properties(excluded_missing_input PROPERTIES REQUIRED_FILES "")
  # When salt-selectfile cannot read the selective instrumentation file
  # (status 2), fparse-llvm gives no warning and runs Flang, and the
  # plugin reports the error in one line, as C does, and fails.
  _salt_add_clang_error_test(fparse_sif_unreadable sif_unreadable.inst.F90
    "lang:Fortran" fortran/hello.f90
    ARGS --tau_select_file=${_no_such_sif}
    LOG "Running:" "ERROR: Cannot open selective instrumentation file: ${_no_such_sif}")
  set_tests_properties(fparse_sif_unreadable PROPERTIES
    FAIL_REGULAR_EXPRESSION "salt-selectfile failed;ERROR:.*\nERROR:")
  # So does a directory.
  _salt_add_clang_error_test(fparse_sif_dir sif_dir.inst.F90
    "lang:Fortran" fortran/hello.f90 ARGS --tau_select_file=${_sif_dir}
    LOG "ERROR: Cannot open selective instrumentation file: ${_sif_dir}")
  set_tests_properties(fparse_sif_dir PROPERTIES
    FAIL_REGULAR_EXPRESSION "salt-selectfile failed;ERROR:.*\nERROR:")
  # The plugin copies a source that the file lists exclude without reading
  # the configuration file, also when Flang parses it for its module files
  # or main program.
  _salt_add_excluded_test(mod_bad_config excluded_mod.f90 MOD excluded_mod
    ARGS --config_file=${_config_parse_error}
    NOT_LOG "${_excluded_flang_error_text}")
  add_sif_test_no_output(bad_config_file_fortran fortran
    tests/fortran/excluded_mod.f90 tests/sif/file_exclude_all.tau
    ARGS --tau_excluded_main_setup=file --config_file=${_config_parse_error})
  # Fortran output keeps the file name of its `#line` lines as written,
  # in the copy of an excluded source and in an instrumented one (#97).
  # Adds test fortran_names_<name>: tests/check_fortran_names.cmake on
  # tests/fortran/line_escape.f90, with the -D options in ARGN.
  function(_salt_add_fortran_names_test name)
    add_test(NAME fortran_names_${name}
      COMMAND ${CMAKE_COMMAND}
        -DSALTFM=${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
        -DSRC=${CMAKE_SOURCE_DIR}/tests/fortran/line_escape.f90
        -DNAME=fortran_names_${name} ${ARGN}
        -P ${CMAKE_SOURCE_DIR}/tests/check_fortran_names.cmake)
    set_tests_properties(fortran_names_${name} PROPERTIES
      LABELS "lang:Fortran;phase:check")
    _salt_in_test_output(instrument fortran_names_${name})
  endfunction()
  _salt_add_fortran_names_test(line_instrumented -DMODE=line)
  _salt_add_fortran_names_test(line_excluded -DMODE=line
    -DSIF=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau)
  # A `"` in a Fortran timer name is doubled, and the split name keeps
  # the pair on one line; a `'`, a `\` and a `$` stay as written. Flang
  # cannot compile the output, as it rejects the `"` in the `#line` line
  # (#97), so gfortran does, if found; else the tests check the output
  # as text.
  # An implicit main program takes its name from the file name; its
  # -DSRC follows, so it replaces, the one of the function.
  find_program(SALT_GFORTRAN NAMES gfortran gfortran-15 gfortran-14
    gfortran-13 gfortran-12)
  set(_names_args -DCONFIG=${CMAKE_SOURCE_DIR}/tests/line_escape.yaml)
  if(SALT_GFORTRAN)
    list(APPEND _names_args -DCOMPILER=${SALT_GFORTRAN})
  else()
    message(STATUS "gfortran not found -- Fortran timer name tests "
      "check the output as text only")
  endif()
  _salt_add_fortran_names_test(timer -DMODE=timer ${_names_args})
  _salt_add_fortran_names_test(dollar -DMODE=dollar ${_names_args})
  set(_implicit -DSRC=${CMAKE_SOURCE_DIR}/tests/fortran/implicit_hi.f90
    -DMODE=implicit ${_names_args})
  _salt_add_fortran_names_test(implicit_quote ${_implicit} [[-DSTEM=a"b]])
  _salt_add_fortran_names_test(implicit_split ${_implicit} [[-DSTEM="b]]
    -DPAD=63)
  _salt_add_fortran_names_test(implicit_apostrophe ${_implicit}
    "-DSTEM=a'b")
  # A byte that is not valid in the locale's encoding, here Latin-1 in
  # UTF-8, does not hide the module.
  _salt_add_excluded_test(mod_latin1 excluded_mod_latin1.f90
    MOD excluded_mod_latin1)
  set_property(TEST excluded_mod_latin1 APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "LC_ALL=set:en_US.UTF-8")
  # --tau_parse_excluded_modules=off, or SALT_PARSE_EXCLUDED_MODULES in
  # any case, skips the parse with a warning; the option has priority.
  _salt_add_excluded_test(mod_off excluded_mod.f90
    ARGS --tau_parse_excluded_modules=off
    LOG "${_excluded_off_text}" NOT_LOG "Running:")
  _salt_add_excluded_test(mod_off_next excluded_mod.f90
    ARGS --tau_parse_excluded_modules off
    LOG "${_excluded_off_text}" NOT_LOG "Running:")
  _salt_add_excluded_test(mod_env_off excluded_mod.f90
    LOG "${_excluded_off_text}" NOT_LOG "Running:")
  _salt_add_excluded_test(mod_option_over_env excluded_mod.f90
    MOD excluded_mod ARGS --tau_parse_excluded_modules=Modules-Only
    NOT_LOG "${_excluded_off_text}")
  foreach(test mod_env_off mod_option_over_env)
    set_property(TEST excluded_${test} APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "SALT_PARSE_EXCLUDED_MODULES=set:OFF")
  endforeach()
  # With `file`, the option has no effect: Flang parses the source.
  _salt_add_excluded_test(mod_file_off excluded_mod.f90 MOD excluded_mod
    ARGS --tau_excluded_main_setup=file --tau_parse_excluded_modules=off
    LOG "Running:" NOT_LOG "${_excluded_off_text}")
  # saltfm passes each value, in any case, as written in lower case.
  foreach(value OFF Modules-Only SUBMODULES-AND-MODULES)
    string(TOLOWER ${value} canonical)
    add_test(NAME saltfm_modules_value_${value}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              --tau_parse_excluded_modules=${value}
              ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set_tests_properties(saltfm_modules_value_${value} PROPERTIES
      PASS_REGULAR_EXPRESSION
        "fparse-llvm --show --tau_parse_excluded_modules=${canonical} .*\ncmd: "
      LABELS "lang:Fortran;phase:smoke")
  endforeach()
  # The option has no synonyms.
  foreach(value always on false)
    add_test(NAME saltfm_modules_unknown_${value}
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              --tau_parse_excluded_modules=${value}
              ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set_tests_properties(saltfm_modules_unknown_${value} PROPERTIES
      PASS_REGULAR_EXPRESSION
        "ERROR: unknown value '${value}' for --tau_parse_excluded_modules\n.*USAGE:"
      FAIL_REGULAR_EXPRESSION "cmd: "
      LABELS "lang:Fortran;phase:smoke")
  endforeach()
  # fparse-llvm takes only the canonical values.
  add_test(NAME fparse_modules_canonical_only
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_parse_excluded_modules=no
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(fparse_modules_canonical_only PROPERTIES
    PASS_REGULAR_EXPRESSION "ERROR: unknown value 'no' for --tau_parse_excluded_modules"
    FAIL_REGULAR_EXPRESSION "cmd: ")
  # Also as the next argument.
  add_test(NAME fparse_modules_next
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            --tau_parse_excluded_modules no
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(fparse_modules_next PROPERTIES
    PASS_REGULAR_EXPRESSION "ERROR: unknown value 'no' for --tau_parse_excluded_modules"
    FAIL_REGULAR_EXPRESSION "cmd: ")
  # Without a value at the end, a warning, and the default.
  add_test(NAME saltfm_modules_no_value
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 --tau_parse_excluded_modules)
  string(CONCAT _modules_no_value
    "WARNING: --tau_parse_excluded_modules needs a value. using\n  the default\n"
    ".*fparse-llvm --show --tau_parse_excluded_modules=modules-only ")
  set_tests_properties(saltfm_modules_no_value PROPERTIES
    PASS_REGULAR_EXPRESSION "${_modules_no_value}")
  add_test(NAME fparse_modules_no_value
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 --tau_parse_excluded_modules)
  set_tests_properties(fparse_modules_no_value PROPERTIES
    PASS_REGULAR_EXPRESSION
      "WARNING: --tau_parse_excluded_modules needs a value. using modules-only\n.*\ncmd: ")
  set_tests_properties(fparse_modules_canonical_only
    fparse_modules_next saltfm_modules_no_value fparse_modules_no_value
    PROPERTIES LABELS "lang:Fortran;phase:smoke")

  # The module and main program checks, with the parse off so that
  # Flang does not run. A comment line does not match, in free and fixed
  # form.
  set(_module_check_dir ${CMAKE_BINARY_DIR}/module_check)
  # Adds test excluded_case_<name> for a source that holds `content`: it
  # must get the warning that its module files are not written only if
  # `module` is 1, and the main program warning only if `program` is 1.
  function(_salt_add_module_check name module program content)
    set(src ${_module_check_dir}/${name}.f90)
    file(WRITE ${src} "${content}")
    set(checks "")
    foreach(kind module program)
      set(text "${_excluded_off_text}")
      if(kind STREQUAL "program")
        set(text "${_excluded_main_warning_text}")
      endif()
      if(${kind})
        list(APPEND checks LOG "${text}")
      else()
        list(APPEND checks NOT_LOG "${text}")
      endif()
    endforeach()
    _salt_add_excluded_test(case_${name} ${src}
      ARGS --tau_parse_excluded_modules=off ${checks})
  endfunction()
  _salt_add_module_check(free_module 1 0 "module m\ncontains\nend module m\n")
  _salt_add_module_check(free_upper 1 0 "MODULE M\nEND MODULE M\n")
  _salt_add_module_check(free_mixed 1 0 "  MoDuLe m\nend module\n")
  _salt_add_module_check(free_indented_tab 1 0 "\tmodule m\nend module\n")
  _salt_add_module_check(free_submodule 1 0 "submodule (p) c\nend submodule\n")
  _salt_add_module_check(free_submodule_noblank 1 0
    "submodule(p) c\nend submodule\n")
  _salt_add_module_check(free_label 1 0 "10 module m\nend module\n")
  _salt_add_module_check(first_line_no_newline 1 0 "module m")
  _salt_add_module_check(crlf_module 1 0 "module m\r\nend module m\r\n")
  _salt_add_module_check(crlf_blank_module 1 0 "module m \r\n")
  # A module statement has a name
  _salt_add_module_check(bare_module 0 0 "module\n")
  _salt_add_module_check(free_comment 0 0 "! module m\nx = 1\n")
  _salt_add_module_check(free_comment_indented 0 0 "   ! module m\nx = 1\n")
  _salt_add_module_check(fixed_comment_upper_c 0 0 "C     MODULE M\n      X = 1\n")
  _salt_add_module_check(fixed_comment_c 0 0 "c     module m\n      x = 1\n")
  _salt_add_module_check(fixed_comment_star 0 0 "*     module m\n      x = 1\n")
  _salt_add_module_check(fixed_comment_bang 0 0 "!     module m\n      x = 1\n")
  _salt_add_module_check(fixed_module 1 0 "      MODULE M\n      END MODULE M\n")
  _salt_add_module_check(fixed_tab_module 1 0 "\tMODULE M\n\tEND MODULE M\n")
  _salt_add_module_check(end_module_only 0 0 "end module m\n")
  _salt_add_module_check(variable_module_x 0 0 "module_x = 1\n")
  _salt_add_module_check(variable_modules 0 0 "modules = 1\n")
  _salt_add_module_check(trailing_comment_module 0 0 "x = 1 ! module m\n")
  _salt_add_module_check(string_module 0 0 "print *, 'module m'\n")
  # A separate module procedure or a module procedure statement has more
  # after the word that follows `module`, so it is not a module.
  _salt_add_module_check(module_subroutine 0 0
    "interface\n  module subroutine work(x)\n  end subroutine\nend interface\n")
  _salt_add_module_check(module_procedure 0 0
    "interface g\n  module procedure f\nend interface g\n")
  _salt_add_module_check(module_comment 1 0 "module m ! the module\nend module\n")
  _salt_add_module_check(module_semicolon 1 0 "module m; end module\n")
  _salt_add_module_check(empty_file 0 0 "")
  # An INCLUDE line or an `#include` can bring in a module
  _salt_add_module_check(include_line 1 0 "include 'm.inc'\n")
  _salt_add_module_check(include_fixed 1 0 "      INCLUDE \"m.inc\"\n")
  _salt_add_module_check(include_noblank 1 0 "include'm.inc'\n")
  _salt_add_module_check(cpp_include 1 0 "#include \"m.inc\"\n")
  _salt_add_module_check(cpp_include_blank 1 0 "  #  include <m.inc>\n")
  _salt_add_module_check(include_comment 0 0 "! include 'm.inc'\nx = 1\n")
  _salt_add_module_check(include_fixed_comment 0 0
    "C     INCLUDE 'm.inc'\n      X = 1\n")
  _salt_add_module_check(include_variable 0 0 "include_x = 1\n")
  # A UTF-8 byte order mark before the first line, which Flang accepts
  string(ASCII 239 187 191 _bom)
  _salt_add_module_check(bom_module 1 0 "${_bom}module m\nend module m\n")
  _salt_add_module_check(free_program 0 1
    "program hello\nprint *, 'hi'\nend program hello\n")
  _salt_add_module_check(free_program_upper 0 1 "PROGRAM HELLO\nEND\n")
  _salt_add_module_check(fixed_program 0 1 "      PROGRAM HELLO\n      END\n")
  _salt_add_module_check(program_comment 0 0 "! program hello\nprint *, 'hi'\nend\n")
  _salt_add_module_check(fixed_program_comment 0 0 "C     PROGRAM HELLO\n      END\n")
  _salt_add_module_check(implicit_main 0 0 "print *, 'hello'\nend\n")
  _salt_add_module_check(program_units_var 0 0 "program_units = 1\n")
  _salt_add_module_check(end_program_only 0 0 "end program hello\n")
  _salt_add_module_check(module_and_program 1 1
    "module m\nend module m\nprogram p\nuse m\nprint *, 'hi'\nend program p\n")
  # With the parse on, a module statement in a comment, in free and in
  # fixed form, does not make Flang run.
  _salt_add_excluded_test(comment_free ${_module_check_dir}/free_comment.f90
    NOT_LOG "Running:")
  _salt_add_excluded_test(comment_fixed
    ${_module_check_dir}/fixed_comment_upper_c.f90 NOT_LOG "Running:")
  # The plugin's copy of an excluded source that Flang parses for its
  # module files has no setup code for its main program.
  _salt_add_excluded_test(mod_program ${_module_check_dir}/module_and_program.f90
    MOD m LOG "Running:")

  # Adds test `name`: _salt_add_clang_error_test on tests/fortran/`src`
  # with the ARGS, LOG, and NOT_LOG, and SALT_BUILD_USES_SALT_FLANG
  # unset. It must write the output with a timer and exit with status 0,
  # or with FAILS, write no output.
  function(_salt_add_fortran_flags_test name src)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "FAILS" "" "ARGS;LOG;NOT_LOG")
    set(out ${name}.inst.F90)
    set(expect EXIT_ZERO CHECK "-s ${out}" OUT "TAU_PROFILE_START")
    if(ARG_FAILS)
      set(expect "")
    endif()
    _salt_add_clang_error_test(${name} ${out} "lang:Fortran" fortran/${src}
      ${expect} ARGS ${ARG_ARGS} LOG ${ARG_LOG} NOT_LOG ${ARG_NOT_LOG})
  endfunction()
  # A generic call resolves to the specific procedure of the kinds that
  # the default kind flags set, as in a Flang build, with and without
  # --tau_build_uses_salt_flang (#40).
  set(_no_specific "No specific subroutine of generic 'show'")
  foreach(mode off on)
    set(_on "")
    if(mode STREQUAL "on")
      set(_on --tau_build_uses_salt_flang)
    endif()
    foreach(kind real integer)
      _salt_add_fortran_flags_test(default_${kind}_8_${mode}
        default-${kind}-8.f90 ARGS ${_on} -fdefault-${kind}-8)
      _salt_add_fortran_flags_test(default_${kind}_8_none_${mode}
        default-${kind}-8.f90 FAILS ARGS ${_on} LOG "${_no_specific}")
    endforeach()
    _salt_add_fortran_flags_test(default_double_8_${mode} default-double-8.f90
      ARGS ${_on} -fdefault-double-8 -fdefault-real-8)
    _salt_add_fortran_flags_test(default_double_8_real_only_${mode}
      default-double-8.f90 FAILS ARGS ${_on} -fdefault-real-8
      LOG "${_no_specific}")
    # Flang refuses -fdefault-double-8 without -fdefault-real-8, which
    # gfortran accepts, so fparse-llvm removes it.
    _salt_add_fortran_flags_test(default_double_8_alone_${mode} hello.f90
      ARGS ${_on} -fdefault-double-8
      LOG "Removed flag without -fdefault-real-8: -fdefault-double-8")
  endforeach()

  # SALT_FORTRAN_VERBOSE turns on the plugin's messages unless it is
  # empty, 0, false, no, or off, in any case.
  foreach(value 1 false Off)
    set(_log NOT_LOG)
    if(value STREQUAL "1")
      set(_log LOG)
    endif()
    _salt_add_fortran_flags_test(fortran_verbose_env_${value} hello.f90
      ${_log} "SALT Instrumentor Plugin finished")
    set_property(TEST fortran_verbose_env_${value} APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "SALT_FORTRAN_VERBOSE=set:${value}")
  endforeach()

  # --tau_build_uses_salt_flang, or SALT_BUILD_USES_SALT_FLANG, makes
  # SALT-FM read the .mod files of a build and its libraries that use its
  # Flang (#40). The fixture installs library module mylib, as the Flang
  # driver writes it, in salt-flang/include, and module files that Flang
  # refuses: one with a changed byte in salt-flang/checksum, one that
  # gfortran writes, if found, in salt-flang/gfortran, and none in
  # salt-flang/none.
  if(_salt_test_flang)
    set(_sf_dir ${SALT_TEST_OUTPUT_DIR}/salt-flang)
    set(_sf_gfortran "")
    if(SALT_GFORTRAN)
      set(_sf_gfortran
        "&& mkdir gfortran && \"$2\" -c -Jgfortran -o gfortran/mylib.o \"$1\"")
    endif()
    add_test(NAME make_mylib_mod
      COMMAND sh -c "rm -rf \"$3\" && mkdir -p \"$3\" && cd \"$3\" &&
        mkdir include checksum none && \"$0\" -c -Jinclude \"$1\" &&
        cp include/mylib.mod checksum &&
        echo '!x' >> checksum/mylib.mod ${_sf_gfortran}"
        ${_salt_test_flang} ${CMAKE_SOURCE_DIR}/tests/fortran/mylib.f90
        "${SALT_GFORTRAN}" ${_sf_dir})
    set_tests_properties(make_mylib_mod PROPERTIES
      FIXTURES_SETUP mylib_mod
      LABELS "lang:Fortran;phase:setup")
    set(_sf_on --tau_build_uses_salt_flang)
    set(_sf_hint
      "Note: the build and its libraries must use LLVM Flang ${LLVM_VERSION_MAJOR}.")
    set(_sf_not_found "Source file 'mylib.saltmod' was not found")
    # Adds test salt_flang_<name>: _salt_add_fortran_flags_test on
    # use-mylib.f90 with ARGN, after the fixture.
    function(_salt_add_salt_flang_test name)
      _salt_add_fortran_flags_test(salt_flang_${name} use-mylib.f90 ${ARGN})
      set_property(TEST salt_flang_${name} APPEND PROPERTY
        FIXTURES_REQUIRED mylib_mod)
    endfunction()
    _salt_add_salt_flang_test(I ARGS ${_sf_on} -I${_sf_dir}/include)
    _salt_add_salt_flang_test(J ARGS ${_sf_on} -J${_sf_dir}/include)
    # The variable turns the option on unless it is empty, 0, false, no,
    # or off, in any case.
    _salt_add_salt_flang_test(env ARGS -I${_sf_dir}/include)
    set_property(TEST salt_flang_env APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "SALT_BUILD_USES_SALT_FLANG=set:1")
    foreach(value empty 0 false NO Off)
      _salt_add_salt_flang_test(env_${value} FAILS ARGS -I${_sf_dir}/include
        LOG "${_sf_not_found}" NOT_LOG "${_sf_hint}")
      string(REPLACE "empty" "" _set "${value}")
      set_property(TEST salt_flang_env_${value} APPEND PROPERTY
        ENVIRONMENT_MODIFICATION "SALT_BUILD_USES_SALT_FLANG=set:${_set}")
    endforeach()
    # Off, Flang reads only .saltmod files, and gives no hint.
    _salt_add_salt_flang_test(off FAILS ARGS -I${_sf_dir}/include
      LOG "${_sf_not_found}" NOT_LOG "${_sf_hint}")
    # When Flang refuses a module file, a hint follows its errors, but not
    # when a module file is missing. The verb in `Cannot <verb> module
    # file`, `read`, `parse`, or `use`, depends on the reason and the
    # Flang version.
    _salt_add_salt_flang_test(hint_checksum FAILS
      ARGS ${_sf_on} -I${_sf_dir}/checksum
      LOG "module 'mylib': File has invalid checksum" "${_sf_hint}")
    _salt_add_salt_flang_test(hint_none FAILS ARGS ${_sf_on} -I${_sf_dir}/none
      LOG "module 'mylib': Source file 'mylib.mod' was not found"
      NOT_LOG "${_sf_hint}")
    _salt_add_fortran_flags_test(salt_flang_hint_missing_and_checksum
      use-missing-and-mylib.f90 FAILS ARGS ${_sf_on} -I${_sf_dir}/checksum
      LOG "module 'missing_module': Source file 'missing_module.mod' was not found"
        "module 'mylib': File has invalid checksum" "${_sf_hint}")
    set_property(TEST salt_flang_hint_missing_and_checksum APPEND PROPERTY
      FIXTURES_REQUIRED mylib_mod)
    # Flang 19 says that the gfortran file has a bad character, Flang 23
    # that it is not a module file, so only the missing .saltmod file of
    # the off mode is excluded.
    if(SALT_GFORTRAN)
      _salt_add_salt_flang_test(hint_gfortran FAILS
        ARGS ${_sf_on} -I${_sf_dir}/gfortran
        LOG " module file for module 'mylib': " "${_sf_hint}"
        NOT_LOG "was not found")
    endif()
    _salt_add_fortran_flags_test(salt_flang_omp_lib use-omp-lib.f90
      ARGS ${_sf_on} -fopenmp NOT_LOG "Removed unknown flag")
    # The parse of a module source of the project keeps the .mod file that
    # the Flang driver wrote, and writes no .saltmod file.
    add_test(NAME salt_flang_project_mod
      COMMAND sh -c "rm -rf salt_flang_project && mkdir salt_flang_project &&
        cd salt_flang_project && \"$0\" -c -J. \"$2\" &&
        cp mylib.mod mylib.mod.orig &&
        \"$1\" ${_sf_on} -J. --tau_output=out.inst.F90 \"$2\" || exit 1
        cmp mylib.mod mylib.mod.orig || { echo 'mylib.mod changed'; exit 1; }
        ! ls *.saltmod > /dev/null 2>&1 ||
          { echo 'Did not expect a .saltmod file'; exit 1; }
        grep -q TAU_PROFILE_START out.inst.F90 ||
          { echo 'Expected a timer in out.inst.F90'; exit 1; }"
        ${_salt_test_flang} ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm
        ${CMAKE_SOURCE_DIR}/tests/fortran/mylib.f90)
    set_tests_properties(salt_flang_project_mod PROPERTIES
      LABELS "lang:Fortran;phase:instrument")
    _salt_in_test_output(instrument salt_flang_project_mod)
    # Flang gets no -module-suffix and not SALT-FM's directory, but the
    # intrinsic module directory of the Flang that runs: the one that
    # Flang 23 prints with `-###`, and none on Flang 19-22, whose -fc1
    # finds it. saltfm passes the option to fparse-llvm only.
    set(_sf_show "cmd: ")
    set(_sf_not_show "-module-suffix;/${SALT_INC_DIR}")
    if(LLVM_VERSION_MAJOR GREATER_EQUAL 23)
      string(CONCAT _sf_show "cmd: [^\n]* -fintrinsic-modules-path "
        "[^ ]*/finclude/flang/[^ ]+ ")
    else()
      list(APPEND _sf_not_show "-fintrinsic-modules-path")
    endif()
    foreach(tool saltfm fparse-llvm)
      string(REPLACE "-llvm" "" _name ${tool}_show_build_uses_salt_flang)
      add_test(NAME ${_name}
        COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/${tool} --show
                ${_sf_on} ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
      set_tests_properties(${_name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${_sf_show}"
        FAIL_REGULAR_EXPRESSION "${_sf_not_show}")
    endforeach()
    # So does a source that the driver does not read as Fortran, such as
    # a .f18 or .inc one, and the .f18 copy of a source that uses
    # iso_c_binding instruments.
    set(_sf_ext_dir ${CMAKE_BINARY_DIR}/salt_flang_ext)
    configure_file(tests/fortran/use-intrinsic-c-binding.f90
      ${_sf_ext_dir}/use-intrinsic-c-binding.f18 COPYONLY)
    foreach(ext f18 inc)
      configure_file(tests/fortran/hello.f90 ${_sf_ext_dir}/hello.${ext}
        COPYONLY)
      add_test(NAME fparse_show_build_uses_salt_flang_${ext}
        COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
                ${_sf_on} ${_sf_ext_dir}/hello.${ext})
      set_tests_properties(fparse_show_build_uses_salt_flang_${ext} PROPERTIES
        PASS_REGULAR_EXPRESSION "${_sf_show}"
        FAIL_REGULAR_EXPRESSION "${_sf_not_show}")
    endforeach()
    _salt_add_clang_error_test(salt_flang_f18 salt_flang_f18.inst.F90
      "lang:Fortran" ${_sf_ext_dir}/use-intrinsic-c-binding.f18 EXIT_ZERO
      CHECK "-s salt_flang_f18.inst.F90" OUT "TAU_PROFILE_START"
      ARGS ${_sf_on})
    add_test(NAME fparse_show_build_uses_salt_flang_env
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/fparse-llvm --show
              ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set_tests_properties(fparse_show_build_uses_salt_flang_env PROPERTIES
      PASS_REGULAR_EXPRESSION "${_sf_show}"
      FAIL_REGULAR_EXPRESSION "${_sf_not_show}")
    add_test(NAME saltfm_show_build_uses_salt_flang_c
      COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
              ${_sf_on} ${CMAKE_SOURCE_DIR}/tests/hello.c)
    set_tests_properties(saltfm_show_build_uses_salt_flang_c PROPERTIES
      PASS_REGULAR_EXPRESSION "^cparse-llvm "
      FAIL_REGULAR_EXPRESSION "build_uses_salt_flang"
      LABELS "lang:C;phase:smoke")
    set_tests_properties(saltfm_show_build_uses_salt_flang
      fparse_show_build_uses_salt_flang fparse_show_build_uses_salt_flang_env
      fparse_show_build_uses_salt_flang_f18
      fparse_show_build_uses_salt_flang_inc
      PROPERTIES LABELS "lang:Fortran;phase:smoke")
    set_property(TEST fparse_show_build_uses_salt_flang_env APPEND PROPERTY
      ENVIRONMENT_MODIFICATION "SALT_BUILD_USES_SALT_FLANG=set:1")
  endif()

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
  # The wrapper does not recognize a `.inc` source, but must refuse it too.
  _salt_add_clang_error_test(fortran_error_self_inc_output
    fortran_error_self.inc "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/fortran_error_self.inc
    COPY_OF fortran/compile_error.f90 ARGS --lang=fortran)
  _salt_add_clang_error_test(fortran_error_ro_output
    fortran_error_ro.inst.F90 "lang:Fortran" fortran/hello.f90 NOT_ROOT
    CHECK "-f fortran_error_ro.inst.F90 -a ! -w fortran_error_ro.inst.F90"
    LOG "ERROR: Could not open output file fortran_error_ro.inst.F90" OUT stale)
  _salt_add_clang_error_test(dev_null_fortran /dev/null "lang:Fortran"
    fortran/hello.f90 EXIT_ZERO CHECK "-c /dev/null")
  # As for C: more than one source with --tau_output is an error before
  # Flang runs, and -o names the output, but not after `--`.
  _salt_add_clang_error_test(multi_source_one_output_fortran
    multi_source_one.inst.F90 "lang:Fortran" fortran/hello.f90
    TAIL ${CMAKE_SOURCE_DIR}/tests/fortran/cubes.f
    LOG "ERROR: -o or --tau_output needs exactly one source file; got 2"
    NOT_LOG "Running:")
  # Unlike C, so is more than one source without an output option: Flang
  # would parse only the last one, and the others would get no output.
  _salt_add_clang_error_test(multi_source_no_output_fortran
    multi_source_no_output_b.inst.F "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_a.f90
    DEFAULT_OUTPUT
    CHECK "! -e multi_source_no_output_a.inst.F90 -a ! -e multi_source_no_output_b.inst.F"
    TAIL ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_b.f
    LOG "ERROR: Fortran instrumentation needs exactly one source file; got 2"
    NOT_LOG "Running:")
  # So are sources with no Fortran extension, with --lang=fortran, with or
  # without --tau_output.
  _salt_add_clang_error_test(multi_source_lang_fortran
    multi_source_lang_b.inst.inc "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_lang_a.inc
    DEFAULT_OUTPUT
    CHECK "! -e multi_source_lang_a.inst.inc -a ! -e multi_source_lang_b.inst.inc"
    ARGS --lang=fortran
    TAIL ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_lang_b.inc
    LOG "ERROR: Fortran instrumentation needs exactly one source file; got 2"
    NOT_LOG "Running:")
  _salt_add_clang_error_test(multi_source_lang_one_output_fortran
    multi_source_lang_one.inst.inc "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_lang_a.inc
    ARGS --lang=fortran
    TAIL ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_lang_b.inc
    LOG "ERROR: -o or --tau_output needs exactly one source file; got 2"
    NOT_LOG "Running:")
  # A file before `--` is a source, also with another source that has a
  # Fortran extension.
  _salt_add_clang_error_test(multi_source_mixed_fortran
    multi_source_lang_b.inst.inc "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_a.f90
    DEFAULT_OUTPUT
    CHECK "! -e multi_source_no_output_a.inst.F90 -a ! -e multi_source_lang_b.inst.inc"
    ARGS --lang=fortran
    TAIL ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_lang_b.inc
    LOG "ERROR: Fortran instrumentation needs exactly one source file; got 2"
    NOT_LOG "Running:")
  # A word that names no file, here the value of an unknown flag, is no
  # source, so the file after it is the input.
  _salt_add_clang_error_test(unknown_value_fortran
    unknown_value_fortran.inst.inc "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/unknown_value_fortran.inc
    DEFAULT_OUTPUT EXIT_ZERO CHECK "-s unknown_value_fortran.inst.inc"
    ARGS --lang=fortran -unknown unknown_value_fortran_ignored
    LOG "Removed unknown flag: unknown_value_fortran_ignored"
    OUT "TAU_PROFILE_START")
  # The source again after `--`, here by another path, is not a second
  # one, but another source after `--` is.
  _salt_add_clang_error_test(repeated_source_fortran
    repeated_source_fortran.inst.F90 "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/repeated_source_fortran.f90
    DEFAULT_OUTPUT EXIT_ZERO CHECK "-s repeated_source_fortran.inst.F90"
    TAIL -- -c repeated_source_fortran.f90 OUT "TAU_PROFILE_START")
  _salt_add_clang_error_test(other_source_after_dashes_fortran
    multi_source_no_output_b.inst.F "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_a.f90
    DEFAULT_OUTPUT
    CHECK "! -e multi_source_no_output_a.inst.F90 -a ! -e multi_source_no_output_b.inst.F"
    TAIL -- -c multi_source_no_output_b.f
    LOG "ERROR: Fortran instrumentation needs exactly one source file; got 2")
  # --version parses no source, so it takes more than one.
  _salt_add_clang_error_test(version_multi_source_fortran
    multi_source_no_output_b.inst.F "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_a.f90
    DEFAULT_OUTPUT EXIT_ZERO
    CHECK "! -e multi_source_no_output_a.inst.F90 -a ! -e multi_source_no_output_b.inst.F"
    ARGS --version
    TAIL ${SALT_TEST_OUTPUT_DIR}/output_error/multi_source_no_output_b.f
    LOG "SALT-FM Version")
  _salt_add_clang_error_test(o_before_source_fortran
    o_before_source_fortran.inst.F90 "lang:Fortran" fortran/hello.f90
    DEFAULT_OUTPUT EXIT_ZERO CHECK "-s o_before_source_fortran.inst.F90"
    ARGS -o o_before_source_fortran.inst.F90 OUT "TAU_PROFILE_START")
  _salt_add_clang_error_test(o_after_source_fortran
    o_after_source_fortran.inst.F90 "lang:Fortran" fortran/hello.f90
    DEFAULT_OUTPUT EXIT_ZERO CHECK "-s o_after_source_fortran.inst.F90"
    TAIL -o o_after_source_fortran.inst.F90 OUT "TAU_PROFILE_START")
  # Guard: fails if fparse-llvm read -o after `--` as the output.
  _salt_add_clang_error_test(o_after_dashes_fortran
    o_after_dashes_fortran.inst.F90 "lang:Fortran"
    ${SALT_TEST_OUTPUT_DIR}/output_error/o_after_dashes_fortran.f90
    DEFAULT_OUTPUT EXIT_ZERO
    CHECK "-s o_after_dashes_fortran.inst.F90 -a ! -e o_after_dashes_fortran.o"
    TAIL -- -o o_after_dashes_fortran.o OUT "TAU_PROFILE_START")
  _salt_add_clang_error_test(o_with_tau_output_fortran
    o_with_tau_output_fortran.inst.F90 "lang:Fortran" fortran/hello.f90
    CHECK "! -e o_with_tau_output_fortran.inst.F90 -a ! -e o_with_tau_output_fortran.o"
    ARGS -o o_with_tau_output_fortran.o
    LOG "ERROR: give -o or --tau_output only once")
  _salt_add_clang_error_test(tau_output_twice_fortran
    tau_output_twice_fortran.inst.F90 "lang:Fortran" fortran/hello.f90
    CHECK "! -e tau_output_twice_fortran.inst.F90 -a ! -e tau_output_twice_fortran.first"
    ARGS --tau_output tau_output_twice_fortran.first
    LOG "ERROR: give -o or --tau_output only once")
  # After `--`, the value of -o, here with a Fortran extension, is the
  # build's output, not a source.
  _salt_add_clang_error_test(o_value_after_dashes_fortran
    o_value_after_dashes_fortran.inst.F90 "lang:Fortran" fortran/hello.f90
    EXIT_ZERO CHECK "-s o_value_after_dashes_fortran.inst.F90"
    TAIL -- -o o_value_after_dashes_fortran.f90 OUT "TAU_PROFILE_START")
  # So is the value of --output, which Clang reads as -o, and of --o, as
  # before `--`.
  _salt_add_clang_error_test(o_long_value_after_dashes_fortran
    o_long_value_after_dashes_fortran.inst.F90 "lang:Fortran"
    fortran/hello.f90 EXIT_ZERO
    CHECK "-s o_long_value_after_dashes_fortran.inst.F90"
    TAIL -- --o o_long_value_after_dashes_fortran.f90
    --output o_long_value_after_dashes_fortran.f08 OUT "TAU_PROFILE_START")
  # But Clang reads -output as -o utput, so the source after it is the
  # input.
  add_test(NAME o_single_dash_output_after_dashes_fortran
    COMMAND ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --show
            --tau_output=out.inst.F90 -- -output
            ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(o_single_dash_output_after_dashes_fortran PROPERTIES
    PASS_REGULAR_EXPRESSION "\ninput file: [^\n]*/tests/fortran/hello[.]f90\n"
    LABELS "lang:Fortran;phase:smoke")
  # As in C, -o=<file>, --o <file>, and --o=<file> name the output, and
  # -o<file> is an error, which removes the setup's stale --tau_output.
  _salt_add_o_form_tests(fortran "lang:Fortran" fortran/hello.f90 .inst.F90)
  _salt_add_clang_error_test(o_joined_fortran o_joined_fortran.out
    "lang:Fortran" fortran/hello.f90
    CHECK "! -e o_joined_fortran.out -a ! -e o_joined_fortran.x"
    ARGS -oo_joined_fortran.x
    LOG "ERROR: Unknown command line argument '-oo_joined_fortran.x'")
  _salt_add_temp_output_test(fortran "lang:Fortran" hello.inst.F90
    ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  _salt_add_write_error_test(write_error_fortran "lang:Fortran"
    ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 write_error.inst.F90)
  _salt_add_write_error_test(write_error_percent_fortran "lang:Fortran"
    ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 a%2Fb/write_error.inst.F90)
  # salt-selectfile copies a source that the file lists exclude.
  _salt_add_write_error_test(write_error_fortran_copy "lang:Fortran;sif"
    ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90 write_error.inst.F90
    --tau_select_file=${CMAKE_SOURCE_DIR}/tests/sif/file_exclude_all.tau)
  # With --tau_output=-, the output goes to standard output, as Flang
  # writes it for `-o -`, and no file is made.
  add_test(NAME stdout_output_fortran
    COMMAND sh -c "cd stdout_output_fortran &&
      \"$0\" \"$@\" > ../stdout_output_fortran.log &&
      grep -q TAU_PROFILE_START ../stdout_output_fortran.log &&
      [ -z \"$(ls)\" ] ||
        { echo 'Expected the output on stdout and no file:'; ls; exit 1; }"
      ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR}/saltfm --tau_output=-
      ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
  set_tests_properties(stdout_output_fortran PROPERTIES
    FIXTURES_REQUIRED clang_error_outputs
    LABELS "lang:Fortran;phase:instrument")
  _salt_in_test_output(output_error stdout_output_fortran)
  # The wrapper's own early exits must not leave a stale output either.
  _salt_add_clang_error_test(fortran_early_exit_output
    fortran_early_exit.inst.F90 "lang:Fortran" fortran/hello.f90
    ARGS --flang_exe=/nonexistent/flang)
  _salt_add_clang_error_test(fortran_early_exit_space_output
    fortran_early_exit_space.inst.F90 "lang:Fortran" fortran/hello.f90
    SPACE ARGS --flang_exe=/nonexistent/flang)
  # Flang run on stdin names the input `<stdin>` (`standard input` in
  # Flang 19), which the plugin cannot reopen; it must stop before it
  # opens the output, so it leaves none.
  if(_salt_test_flang)
    add_test(NAME fortran_plugin_stdin_output
      COMMAND sh -c "rm -f plugin_stdin.inst.F90
        \"$0\" -fc1 -load \"$1\" -plugin salt-instrument \\
          -o plugin_stdin.inst.F90 < \"$2\" > plugin_stdin.log 2>&1; rc=$?
        cat plugin_stdin.log
        [ $rc -ge 1 ] && [ $rc -le 127 ] ||
          { echo \"Expected an exit status from 1 to 127, got $rc\"; exit 1; }
        [ ! -e plugin_stdin.inst.F90 ] ||
          { echo 'Expected no plugin_stdin.inst.F90'; exit 1; }
        grep -qF 'ERROR: Could not open input file ' plugin_stdin.log ||
          { echo 'Expected the input error'; exit 1; }"
        ${_salt_test_flang} $<TARGET_FILE:salt-flang-plugin>
        ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
    set_tests_properties(fortran_plugin_stdin_output PROPERTIES
      ENVIRONMENT "SALT_FORTRAN_CONFIG_FILE=${_tau_config_path}"
      LABELS "lang:Fortran;phase:instrument")
    _salt_in_test_output(output_error fortran_plugin_stdin_output)
    # Without -o, the plugin names its output as Flang does: the input
    # with the extension `inst.F90`, in the input's directory, not the
    # working directory, and leaves no temporary file.
    add_test(NAME fortran_plugin_default_output
      COMMAND sh -c "cd plugin_default_cwd &&
        \"$0\" -fc1 -load \"$1\" -plugin salt-instrument \\
          ../plugin_default.f90 &&
        grep -q TAU_PROFILE_START ../plugin_default.inst.F90 || exit 1
        set -- ../plugin_default.inst.F90-*.tmp
        [ -z \"$(ls)\" ] && [ ! -e \"$1\" ] ||
          { echo 'Expected no file here and no temporary file:'; ls . ..; exit 1; }"
        ${_salt_test_flang} $<TARGET_FILE:salt-flang-plugin>)
    set_tests_properties(fortran_plugin_default_output PROPERTIES
      ENVIRONMENT "SALT_FORTRAN_CONFIG_FILE=${_tau_config_path}"
      FIXTURES_REQUIRED clang_error_outputs
      LABELS "lang:Fortran;phase:instrument")
    _salt_in_test_output(output_error fortran_plugin_default_output)
  endif()
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
    --flang_exe=/nonexistent/flang fortran_other-x.inc)
  _salt_add_config_read_error_tests(fortran "lang:Fortran" fortran/hello.f90)
  _salt_add_no_path_tests(fortran "lang:Fortran" fortran/hello.f90)
  _salt_add_config_error_test(no_Fortran_fortran "lang:Fortran"
    fortran/hello.f90 ${_config_errors}/no_Fortran.yaml
    "ERROR: 'Fortran' key not found ${_in_config}/no_Fortran.yaml")
  _salt_add_config_error_test(scalar_Fortran_fortran "lang:Fortran"
    fortran/hello.f90 ${_config_errors}/scalar_Fortran.yaml
    "ERROR: 'Fortran' ${_in_config}/scalar_Fortran.yaml must be a map")
  foreach(key program_insert procedure_begin_insert procedure_end_insert)
    _salt_add_config_error_test(no_${key}_fortran "lang:Fortran"
      fortran/hello.f90 ${_config_errors}/no_${key}.yaml
      "ERROR: '${key}' key not found under 'Fortran' ${_in_config}/no_${key}.yaml")
  endforeach()
  foreach(name scalar_procedure_end_insert map_program_setup_insert)
    string(REGEX MATCH "^[a-z]+_(.*)" _ ${name})
    _salt_add_config_error_test(${name}_fortran "lang:Fortran"
      fortran/hello.f90 ${_config_errors}/${name}.yaml
      "ERROR: '${CMAKE_MATCH_1}' under 'Fortran' ${_in_config}/${name}.yaml ${_list}")
  endforeach()
  # A source with nothing to instrument needs no configuration file: the
  # plugin writes its module file and a copy with a #line line.
  set(_data_only ${CMAKE_BINARY_DIR}/fortran_ext/data_only.f90)
  file(WRITE ${_data_only} "module data_only\n  integer :: x\nend module data_only\n")
  _salt_add_clang_error_test(config_ok_data_only_fortran
    config_ok_data_only_fortran.out "lang:Fortran" ${_data_only} EXIT_ZERO
    CHECK "-s config_ok_data_only_fortran.out -a -s data_only.saltmod"
    ARGS --config_file=${_config_errors}/no_such_file.yaml
    OUT "#line 1 \\\"${_data_only}\\\"" "module data_only")
  # A Fortran run needs no top-level C and C++ keys.
  _salt_add_clang_error_test(config_ok_fortran config_ok_fortran.out
    "lang:Fortran" fortran/hello.f90 EXIT_ZERO CHECK "-s config_ok_fortran.out"
    ARGS --config_file=${_config_errors}/fortran.yaml
    OUT "call TAU_PROFILE_START(tauProfileTimer)")
  # Flang -fc1 reads neither .ftn nor .FTN as fixed form, nor
  # predefines macros such as __flang__ for .FTN, so fparse-llvm asks it
  # to (#102).
  configure_file(tests/fortran/myhi.f ${CMAKE_BINARY_DIR}/fortran_ext/myhi.ftn
    COPYONLY)
  configure_file(tests/fortran/fixed_cpp.F
    ${CMAKE_BINARY_DIR}/fortran_ext/fixed_cpp.FTN COPYONLY)
  foreach(src myhi.ftn fixed_cpp.FTN)
    string(REPLACE "." "_" _name ${src})
    _salt_add_clang_error_test(fortran_ext_${_name} fortran_ext_${_name}.out
      "lang:Fortran" ${CMAKE_BINARY_DIR}/fortran_ext/${src} EXIT_ZERO
      CHECK "-s fortran_ext_${_name}.out" OUT "TAU_PROFILE_START")
  endforeach()
  # Without --tau_output, the output of a fixed-form source ends in .F,
  # and that of a free-form source in .F90, which gfortran and Flang
  # read as preprocessed sources of the same form.
  foreach(ext f for ftn f77 fpp FOR f90 f95 f03 f08 f18 F08)
    set(_src ${CMAKE_SOURCE_DIR}/tests/fortran/myhi.f)
    set(_out_ext F)
    if(ext MATCHES "^[fF](90|95|03|08|18)$")
      set(_src ${CMAKE_SOURCE_DIR}/tests/fortran/hello.f90)
      set(_out_ext F90)
    endif()
    # Names differ in more than case, for case-insensitive file systems
    string(TOLOWER ${ext} _name)
    if(NOT ext STREQUAL _name)
      string(APPEND _name _upper)
    endif()
    set(_name fortran_default_output_${_name})
    configure_file(${_src} ${CMAKE_BINARY_DIR}/fortran_ext/${_name}.${ext}
      COPYONLY)
    _salt_add_clang_error_test(${_name} ${_name}.inst.${_out_ext}
      "lang:Fortran" ${CMAKE_BINARY_DIR}/fortran_ext/${_name}.${ext}
      DEFAULT_OUTPUT EXIT_ZERO CHECK "-s ${_name}.inst.${_out_ext}"
      OUT "TAU_PROFILE_START")
  endforeach()

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
  # matches. The optional OUTPUT must match the program's output, the
  # optional PROFILE names the profile (default: profile.0.0.0), and the
  # other arguments are extra compile options.
  function(_salt_add_tau_main_test name src sif pass main_timer)
    cmake_parse_arguments(PARSE_ARGV 5 ARG "" "OUTPUT;PROFILE" "")
    set(profile profile.0.0.0)
    if(ARG_PROFILE)
      set(profile ${ARG_PROFILE})
    endif()
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
        ENVIRONMENT_MODIFICATION
          "PATH=path_list_prepend:${TAU_BIN_DIR}"
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
        COMMAND ${CMAKE_COMMAND} -E cat ${profile}
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
  # the source. With `file`, which the test sets in
  # SALT_EXCLUDED_MAIN_SETUP in the environment of TAU's compiler
  # wrapper, its main has no timer but sets the node, so the program,
  # with the timer of keep_routine in sif_nomain.c, writes profile.0.0.0.
  foreach(ext c cpp)
    _salt_add_tau_main_test(sif_file_exclude_main_${ext}
      tests/sif_file_name.${ext} tests/sif/file_exclude_file_name.tau
      keep_routine "int main[(]"
      OUTPUT "file: [^\n]*/tests/sif_file_name[.]${ext}\n"
      ${CMAKE_SOURCE_DIR}/tests/sif_nomain.c)
    foreach(comp IN LISTS SALT_COMPILERS_TO_TEST)
      set_property(TEST compile_${comp}_sif_file_exclude_main_${ext}
        APPEND PROPERTY ENVIRONMENT_MODIFICATION "SALT_EXCLUDED_MAIN_SETUP=set:file")
    endforeach()
  endforeach()
  # By default, its main gets no setup code, so the program
  # sets no node and writes profile.-1.0.0.
  _salt_add_tau_main_test(sif_file_exclude_main_default_c
    tests/sif_file_name.c tests/sif/file_exclude_file_name.tau
    keep_routine "int main[(]" PROFILE profile.-1.0.0
    ${CMAKE_SOURCE_DIR}/tests/sif_nomain.c)
  if(TEST_FORTRAN)
    _salt_add_tau_main_test(sif_file_exclude_main_fortran
      tests/fortran/sif_file_name.f90 tests/sif/file_exclude_file_name.tau
      keep_routine "[Ss][Ii][Ff]_[Ff][Ii][Ll][Ee]_[Nn][Aa][Mm][Ee] .[{]"
      ${CMAKE_SOURCE_DIR}/tests/fortran/sif_keep.f90)
    foreach(comp IN LISTS SALT_FORTRAN_COMPILERS_TO_TEST)
      set_property(TEST compile_${comp}_sif_file_exclude_main_fortran
        APPEND PROPERTY ENVIRONMENT_MODIFICATION "SALT_EXCLUDED_MAIN_SETUP=set:file")
    endforeach()
  endif()
endif()

# A SALT_* variable in the developer's environment must not reach a test
# that does not set it, so each test unsets every SALT_* name in the
# sources that it does not set.
# The list is deliberately broad: it also has include guards, macros, and
# @VAR@ placeholders, which are harmless to unset.
file(GLOB _salt_sources ${CMAKE_SOURCE_DIR}/src/* ${CMAKE_SOURCE_DIR}/include/*)
set(_salt_vars "")
foreach(src IN LISTS _salt_sources)
  file(STRINGS ${src} _lines REGEX "SALT_[A-Z0-9_]+")
  string(REGEX MATCHALL "SALT_[A-Z0-9_]+" _names "${_lines}")
  list(APPEND _salt_vars ${_names})
endforeach()
list(REMOVE_DUPLICATES _salt_vars)
# env_isolation checks that the loop below reaches every test and name.
string(REPLACE ";" "," _salt_vars_arg "${_salt_vars}")
add_test(NAME env_isolation
  COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${CMAKE_BINARY_DIR}
    -DNAMES=${_salt_vars_arg} -DCTEST_CONFIGURATION_TYPE=$<CONFIG>
    -P ${CMAKE_SOURCE_DIR}/tests/check_env_isolation.cmake)
set_tests_properties(env_isolation PROPERTIES LABELS "phase:check")
get_property(_salt_tests DIRECTORY PROPERTY TESTS)
foreach(test IN LISTS _salt_tests)
  get_test_property(${test} ENVIRONMENT _env)
  get_test_property(${test} ENVIRONMENT_MODIFICATION _env_mod)
  set(_unset "")
  foreach(var IN LISTS _salt_vars)
    if(NOT ";${_env};${_env_mod}" MATCHES ";${var}=")
      list(APPEND _unset "${var}=unset:")
    endif()
  endforeach()
  set_property(TEST ${test} APPEND PROPERTY ENVIRONMENT_MODIFICATION "${_unset}")
endforeach()
