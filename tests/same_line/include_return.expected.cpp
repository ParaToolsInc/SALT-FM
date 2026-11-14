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
    timer_start("int decl(void)  [{include_return.cpp} {11,1}-{15,1}]");
#include "include_decl.inc"
	{ int inst_ret_val =  k; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{include_return.cpp} {16,1}-{19,1}]");
	{ int inst_ret_val =  early(4) + decl(); timer_stop(); return inst_ret_val; }
}
