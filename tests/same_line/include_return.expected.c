#line 1 "include_return.c"
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
#line 19
    timer_start("int decl(void)  [{include_return.c} {18,1}-{22,1}]");
#line 19
#include "include_decl.inc"
#line 21
	{ int inst_ret_val =  k; timer_stop(); return inst_ret_val; }
#line 21
#line 22
#line 22
}
int main(void)
{
#line 24
    timer_start("int main(void)  [{include_return.c} {23,1}-{28,1}]");
#line 24
    int n = 3;
    ends(&n);
#line 27
	{ int inst_ret_val =  early(n) + opens() + decl(); timer_stop(); return inst_ret_val; }
#line 27
#line 28
#line 28
}
