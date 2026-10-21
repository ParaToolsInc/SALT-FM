// The ryml implementation, compiled once for cparse-llvm and the Flang plugin
#define RYML_SINGLE_HDR_DEFINE_NOW
#define RYML_SHARED

#include "config_file.hpp"

#include <cstdlib>
#include <cstring>

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

// Replaces ryml's error handler, which aborts: reports the error, at its
// position in the configuration file when ryml gives one, else with the
// file name `path` (as for an undefined alias), and exits.
[[noreturn]] static void exit_on_yaml_error(const char *msg, size_t len, ryml::Location loc, void *path)
{
    llvm::StringRef text(msg, len);
    text.consume_front("ERROR: ");
    llvm::errs() << "ERROR: ";
    if (loc)
    {
        llvm::errs() << llvm::StringRef(loc.name.str, loc.name.len) << ":" << loc.line << ":" << loc.col << ": ";
    }
    else
    {
        llvm::errs() << *static_cast<const std::string *>(path) << ": ";
    }
    llvm::errs() << text.rtrim() << "\n";
    std::exit(EXIT_FAILURE);
}

ryml::Tree read_config_file(const std::string &path)
{
    // The handler stays set after the return, so it gets a copy of `path`
    static std::string error_path;
    error_path = path;
    ryml::set_callbacks(ryml::Callbacks(&error_path, nullptr, nullptr, exit_on_yaml_error));
    auto buffer = llvm::MemoryBuffer::getFile(path);
    if (!buffer)
    {
        llvm::errs() << "ERROR: Could not read configuration file " << path << ": " << buffer.getError().message()
                     << "\n";
        std::exit(EXIT_FAILURE);
    }
    const llvm::StringRef text = (*buffer)->getBuffer();
    // The returned copy of the document of a stream points to the strings
    // of this tree, so it lives until the exit
    static ryml::Tree tree;
    tree = ryml::parse_in_arena(ryml::to_csubstr(path), ryml::csubstr(text.data(), text.size()));
    tree.resolve();
    const ryml::id_type root = tree.root_id();
    if (!tree.is_stream(root))
    {
        return tree;
    }
    // A file that starts with `---` is a stream
    if (tree.num_children(root) != 1)
    {
        llvm::errs() << "ERROR: The configuration file " << path << " must hold one YAML document\n";
        std::exit(EXIT_FAILURE);
    }
    ryml::Tree document;
    document.duplicate_contents(&tree, tree.first_child(root), document.root_id());
    return document;
}

// The child `key` of the map `node`, or an invalid node when `node` is not
// a map or has no `key`.
static ryml::ConstNodeRef find_config_key(ryml::ConstNodeRef node, const char *key)
{
    // find_child(), unlike [], does not assert that the key exists, but
    // asserts a map
    return node.is_map() ? node.find_child(ryml::to_csubstr(key)) : ryml::ConstNodeRef();
}

// Whether `node` is a list of strings, or empty, as code lines must be
static bool is_string_list(ryml::ConstNodeRef node)
{
    if (!node.is_seq())
    {
        return node.has_val() && node.val_is_null();
    }
    for (ryml::ConstNodeRef child : node.children())
    {
        if (!child.is_val())
        {
            return false;
        }
    }
    return true;
}

ryml::ConstNodeRef check_config_key(ryml::ConstNodeRef node, const char *key, const std::string &path,
                                    const char *section, bool optional)
{
    const ryml::ConstNodeRef value = find_config_key(node, key);
    std::string where = " in the configuration file " + path;
    if (section)
    {
        where = " under '" + std::string(section) + "'" + where;
    }
    if (value.invalid())
    {
        if (optional)
        {
            return value;
        }
        llvm::errs() << "ERROR: '" << key << "' key not found" << where << "\n";
        std::exit(EXIT_FAILURE);
    }
    const char *type = "list of strings";
    bool valid = is_string_list(value);
    if (strcmp(key, "instrumentation") == 0)
    {
        type = "string";
        valid = value.has_val();
    }
    else if (strcmp(key, "Fortran") == 0)
    {
        type = "map";
        valid = value.is_map();
    }
    if (!valid)
    {
        llvm::errs() << "ERROR: '" << key << "'" << where << " must be a " << type << "\n";
        std::exit(EXIT_FAILURE);
    }
    return value;
}
