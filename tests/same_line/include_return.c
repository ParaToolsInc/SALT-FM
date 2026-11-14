// A function with a `return` statement or a brace in an included file
// gets no timer, with a warning that names it and the file, and the
// other functions keep their timers (#116). A function that includes
// only declarations keeps its timer.
int early(int n)
{
#include "include_return.inc"
    return n + 1;
}
void ends(int *n)
{
    *n += 1;
#include "include_close.inc"
int opens(void)
#include "include_open.inc"
    return 1;
}
int decl(void)
{
#include "include_decl.inc"
    return k;
}
int main(void)
{
    int n = 3;
    ends(&n);
    return early(n) + opens() + decl();
}
