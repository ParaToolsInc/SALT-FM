#ifndef SALT_OUTPUT_FILE_HPP
#define SALT_OUTPUT_FILE_HPP

#include <optional>
#include <string>

// The file to write output `path` to: a new temporary file in the
// directory of `path`, which keepOutputs() renames to `path`, or `path`
// itself when it exists and is not a regular file, such as /dev/null.
// Returns nothing after an error that names `path`, as when `path` is a
// regular file with no write access. An exit before keepOutputs()
// removes the temporary files, so a failed run leaves no partial output.
std::optional<std::string> openOutput(const std::string &path);

// Renames the temporary files of openOutput() to their outputs, in the
// order they were opened. Returns false after an error, which stops it
// and removes the outputs it renamed.
bool keepOutputs();

#endif
