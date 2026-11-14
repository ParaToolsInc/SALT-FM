// A function with a `return` statement in an included file gets no
// timer, with a warning that names it and the file, and the other
// functions keep their timers (#116). A function that includes only
// declarations keeps its timer. The C++ API puts timer code only after
// the opening brace, so all functions keep their timers with it.
int early(int n)
{
    timer_scope t("int early(int)  [{include_return.cpp} {6,1}-{10,1}]");
#include "include_return.inc"
    return n + 1;
}
int decl(void)
{
    timer_scope t("int decl(void)  [{include_return.cpp} {11,1}-{15,1}]");
#include "include_decl.inc"
    return k;
}
int main(void)
{
    timer_scope t("int main(void)  [{include_return.cpp} {16,1}-{19,1}]");
    return early(4) + decl();
}
