// A main function with a `return` statement in an included file keeps
// its setup code, but gets no timer (#116)
int main(int argc, char **argv)
{
    int n = argc;
    (void)argv;
#include "include_return.inc"
    return 0;
}
