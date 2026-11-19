// A `return` statement with a directive before its `;`, such as `#if`,
// or an `#include` that gives the `;`, gets no timer, with a warning
// that names it, and a later function keeps its timer (#116)
int cond(int x)
{
    return x
#if 1
    ;
#endif
}
int incl(int x)
{
    return x + 1
#include "include_semi.inc"
}
int later(int x) { return x + 1; }
int main(void)
{
    return cond(1) + incl(2) + later(3);
}
