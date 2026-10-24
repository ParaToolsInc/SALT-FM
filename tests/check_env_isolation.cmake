# Fails unless each test in BINARY_DIR/CTestTestfile.cmake sets, or
# unsets, each of the comma-separated NAMES in its ENVIRONMENT or
# ENVIRONMENT_MODIFICATION, so that a SALT_* variable in the developer's
# environment reaches no test that does not set it. This reads the file
# that ctest reads, with add_test and set_tests_properties replaced by
# functions that record each test and its environment; it runs no test
# and writes no Testing/ log. (`ctest --show-only=json-v1` gives the
# same, but string(JSON) parses all of it on each call, which takes
# over a minute.) A multi-config generator writes the tests of each
# configuration under `if(CTEST_CONFIGURATION_TYPE ...)`, which ctest
# sets from `-C`, so pass the same configuration.
#
# Usage: cmake -DBINARY_DIR=<dir> -DNAMES=<name>[,<name>...]
#          [-DCTEST_CONFIGURATION_TYPE=<config>] -P check_env_isolation.cmake

if(NAMES STREQUAL "")
  message(FATAL_ERROR "Expected SALT_* names")
endif()
string(REPLACE "," ";" _names "${NAMES}")
# Records test `name`.
function(add_test name)
  set_property(GLOBAL APPEND PROPERTY _salt_tests "${name}")
endfunction()
# Records the environment of test `name`. CMake writes one test name and
# then PROPERTIES; each ARGV<n> keeps a list value whole.
function(set_tests_properties name)
  math(EXPR last "${ARGC} - 1")
  foreach(idx RANGE 2 ${last} 2)
    math(EXPR value "${idx} + 1")
    if(ARGV${idx} MATCHES "^ENVIRONMENT(_MODIFICATION)?$")
      set_property(GLOBAL APPEND PROPERTY _salt_env_${name} "${ARGV${value}}")
    endif()
  endforeach()
endfunction()
include(${BINARY_DIR}/CTestTestfile.cmake)
get_property(_tests GLOBAL PROPERTY _salt_tests)
list(LENGTH _tests _count)
if(_count EQUAL 0)
  message(FATAL_ERROR "Expected tests in ${BINARY_DIR}/CTestTestfile.cmake")
endif()
set(_missing 0)
foreach(test IN LISTS _tests)
  get_property(_test_env GLOBAL PROPERTY _salt_env_${test})
  foreach(var IN LISTS _names)
    if(NOT ";${_test_env};" MATCHES ";${var}=")
      math(EXPR _missing "${_missing} + 1")
      if(_missing LESS_EQUAL 10)
        message("${test} neither sets nor unsets ${var}")
      endif()
    endif()
  endforeach()
endforeach()
if(_missing GREATER 0)
  message(FATAL_ERROR
    "${_missing} times a test neither sets nor unsets a SALT_* name")
endif()
list(LENGTH _names _count_names)
message("All ${_count} tests set or unset all ${_count_names} SALT_* names")
