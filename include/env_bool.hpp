#ifndef SALT_ENV_BOOL_HPP
#define SALT_ENV_BOOL_HPP

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// Whether on/off environment variable `name` is on. It is off when unset,
// empty, 0, false, no, or off, in any case, and on for any other value.
// env_bool in src/salt-common.sh applies the same rule.
inline bool getEnvBool(const char *name) {
    const char *val = std::getenv(name);
    if (val == nullptr) {
        return false;
    }
    std::string s(val);
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return !(s.empty() || s == "0" || s == "false" || s == "no" || s == "off");
}

#endif
