#ifndef SALT_CONFIG_FILE_HPP
#define SALT_CONFIG_FILE_HPP

#include <string>

#include <ryml_all.hpp>

// The parsed configuration file `path`, with its YAML aliases resolved;
// for a file that starts with `---`, its one document. Exits with an error
// when the file cannot be read, does not parse, or holds more than one
// document; a parse error names the file, line, and column. Call it once.
ryml::Tree read_config_file(const std::string &path);

// The child `key` of the map `node` of the configuration file `path`, or
// an invalid node when `optional` and there is no `key`. `section`, unless
// null, is the key of `node`, for the error messages. Exits with an error
// naming `key` and `path` when `key` is missing and not `optional`, or its
// value is not a list of strings (a string for `instrumentation`, a map
// for `Fortran`).
ryml::ConstNodeRef check_config_key(ryml::ConstNodeRef node, const char *key, const std::string &path,
                                    const char *section = nullptr, bool optional = false);

#endif
