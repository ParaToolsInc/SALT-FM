// A function whose `return` statement ends with a `;` from a macro, or
// that calls a macro of a header with a `return` statement, gets no
// timer, with a warning that names it, and a later function keeps its
// timer (#116)
#include "macro_return.h"
#define ZERO 0;
int zero(void) {
  return ZERO
}
int check(int n)
{
    CHECK(n);
    return n;
}
int later(int n) {
    timer_start("int later(int)  [{macro_return.c} {15,1}-{15,34}]");
	{ int inst_ret_val =  n + 1; timer_stop(); return inst_ret_val; }
}
// A function whose `return` statement ends at a `;` inside a macro call
// gets no timer either, even with a `;` after the call
#define RESULT 0; ++hits
int hits;
int result(void) {
  return RESULT;
}
int main(void)
{
    timer_start("int main(void)  [{macro_return.c} {23,1}-{27,1}]");
    int n = result();
	{ int inst_ret_val =  zero() + check(2) + later(3) + n + hits; timer_stop(); return inst_ret_val; }
}
