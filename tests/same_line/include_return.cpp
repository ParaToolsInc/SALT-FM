// A function with a `return` statement in an included file gets no
// timer, with a warning that names it and the file, and the other
// functions keep their timers (#116). A function that includes only
// declarations keeps its timer. The C++ API puts timer code only after
// the opening brace, so all functions keep their timers with it.
int early(int n)
{
#include "include_return.inc"
    return n + 1;
}
int decl(void)
{
#include "include_decl.inc"
    return k;
}
int main(void)
{
    return early(4) + decl();
}
