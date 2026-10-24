#!/usr/bin/env bash
# Shell code that CMake inlines into saltfm and fparse-llvm, at their
# SALT_COMMON_SH placeholder, without this first line (#105). It must
# work with bash 3.2.

# Whether on/off environment variable `$1` is on: returns 1 when it is
# unset, empty, 0, false, no, or off, in any case, and 0 for any other
# value. getEnvBool in include/env_bool.hpp applies the same rule.
function env_bool {
    case ${!1:-} in
        '' | 0 | [Ff][Aa][Ll][Ss][Ee] | [Nn][Oo] | [Oo][Ff][Ff]) return 1 ;;
    esac
}
