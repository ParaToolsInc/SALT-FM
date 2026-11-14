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
int later(int x) {
    timer_start("int later(int)  [{return_directive.c} {16,1}-{16,34}]");
	{ int inst_ret_val =  x + 1; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_directive.c} {17,1}-{20,1}]");
	{ int inst_ret_val =  cond(1) + incl(2) + later(3); timer_stop(); return inst_ret_val; }
}
