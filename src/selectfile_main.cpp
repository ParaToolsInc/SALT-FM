// salt-selectfile: tells fparse-llvm, before Flang runs, whether the file
// lists of a selective instrumentation file exclude a source, and copies a
// source that SALT-FM does not instrument.
//
// Usage: salt-selectfile <sif> <source>
//          exits with 0 if the file lists include <source>, 1 if they
//          exclude it, or 2 after an error
//        salt-selectfile --copy <source> <output>
//          writes `#line 1 "<source>"` and then <source> to <output>;
//          exits with 0, or 2 after an error
#include <cstdio>
#include <cstring>

#include "selectfile.hpp"

int main(int argc, char **argv)
{
    if (argc == 4 && std::strcmp(argv[1], "--copy") == 0)
    {
        return copySource(argv[2], argv[3], false) ? 0 : 2;
    }
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <sif> <source>\n       %s --copy <source> <output>\n", argv[0],
                     argv[0]);
        return 2;
    }
    if (!processInstrumentationRequests(argv[1]))
    {
        return 2;
    }
    return instrumentFile(argv[2]) ? 0 : 1;
}
