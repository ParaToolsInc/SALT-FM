// Outputs written to temporary files and renamed into place, for
// cparse-llvm, the Flang plugin, and salt-selectfile
#include "output_file.hpp"

#include <cstdlib>
#include <utility>
#include <vector>

#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"

// Each temporary file of openOutput() and its output, until keepOutputs()
static std::vector<std::pair<std::string, std::string>> pending;

// Removes the temporary files at an exit before keepOutputs().
// Each name is unregistered first, so that a signal never removes a
// file that another process made with the name after us.
static void discardOutputs()
{
    for (const auto &output : pending)
    {
        llvm::sys::DontRemoveFileOnSignal(output.first);
        llvm::sys::fs::remove(output.first);
    }
    pending.clear();
}

std::optional<std::string> openOutput(const std::string &path)
{
    llvm::sys::fs::file_status status;
    const bool exists = !llvm::sys::fs::status(path, status);
    // A rename onto a device fails, or, as root, replaces the device
    if (exists && !llvm::sys::fs::is_regular_file(status))
    {
        return path;
    }
    // A rename replaces a read-only file, which an open for writing refuses
    if (exists)
    {
        if (std::error_code err = llvm::sys::fs::access(path, llvm::sys::fs::AccessMode::Write))
        {
            llvm::errs() << "ERROR: Could not open output file " << path << ": " << err.message() << "\n";
            return std::nullopt;
        }
    }
    // A random name, made with O_EXCL, so that processes on nodes that
    // share the file system never take or remove each other's file
    std::string name;
    std::error_code err;
    for (int tries = 0; tries < 128; ++tries)
    {
        name = path + "-" + llvm::utohexstr(llvm::sys::Process::GetRandomNumber(), true, 8) + ".tmp";
        int fd;
        err = llvm::sys::fs::openFileForWrite(name, fd, llvm::sys::fs::CD_CreateNew, llvm::sys::fs::OF_None, 0666);
        if (err != std::errc::file_exists)
        {
            if (!err)
            {
                llvm::sys::Process::SafelyCloseFileDescriptor(fd);
            }
            break;
        }
    }
    if (err)
    {
        llvm::errs() << "ERROR: Could not create a temporary file next to output file " << path << ": "
                     << err.message() << "\n";
        return std::nullopt;
    }
    llvm::sys::RemoveFileOnSignal(name);
    // Registered after `pending` exists, so it runs before its destructor
    [[maybe_unused]] static const int registered = std::atexit(discardOutputs);
    pending.emplace_back(name, path);
    return name;
}

bool keepOutputs()
{
    for (size_t i = 0; i < pending.size(); ++i)
    {
        const auto &[name, path] = pending[i];
        llvm::sys::DontRemoveFileOnSignal(name);
        if (std::error_code err = llvm::sys::fs::rename(name, path))
        {
            llvm::errs() << "ERROR: Could not write output file " << path << ": " << err.message() << "\n";
            llvm::sys::fs::remove(name);
            // Remove the outputs renamed before, so the run leaves none;
            // the exit removes the temporary files after this one
            for (size_t j = 0; j < i; ++j)
            {
                llvm::sys::fs::remove(pending[j].second);
            }
            pending.erase(pending.begin(), pending.begin() + i + 1);
            return false;
        }
    }
    pending.clear();
    return true;
}
