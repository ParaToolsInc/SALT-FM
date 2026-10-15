// Checks a TAU selective instrumentation reference case (tests/sif/tau/)
// against SALT-FM's parser and matcher: reads <case>.tau, then checks
// each `func: <name> => yes|no` and `file: <path> => yes|no` line of
// <case>.expect (yes = instrumented). Other lines are ignored. Fails if a
// check fails or the .expect file has no checks.
#include <fstream>
#include <iostream>
#include <string>

#include "selectfile.hpp"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " <case>.tau <case>.expect\n";
        return 2;
    }
    if (!processInstrumentationRequests(argv[1]))
    {
        return 2;
    }
    std::ifstream expect(argv[2]);
    if (!expect)
    {
        std::cerr << "ERROR: Cannot open " << argv[2] << "\n";
        return 2;
    }
    int checks = 0;
    int failures = 0;
    std::string line;
    while (std::getline(expect, line))
    {
        const size_t colon = line.find(": ");
        const size_t arrow = line.rfind(" => ");
        if (colon == std::string::npos || arrow == std::string::npos || arrow < colon)
        {
            continue;
        }
        const std::string kind = line.substr(0, colon);
        const std::string name = line.substr(colon + 2, arrow - colon - 2);
        const bool want = line.substr(arrow + 4) == "yes";
        bool got;
        if (kind == "func")
        {
            got = instrumentRoutine(name);
        }
        else if (kind == "file")
        {
            got = instrumentFile(name);
        }
        else
        {
            continue;
        }
        ++checks;
        if (got != want)
        {
            std::cout << "FAIL " << kind << " '" << name << "': expected " << (want ? "yes" : "no") << "\n";
            ++failures;
        }
    }
    std::cout << checks << " checks, " << failures << " failed\n";
    return (failures > 0 || checks == 0) ? 1 : 0;
}
