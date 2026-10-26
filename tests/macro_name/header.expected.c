#include "header.h"
void timer_start(const char *name);
void timer_stop(void);
RET f(void) {
    timer_start("int f(void)  [{header.c} {4,1}-{6,1}]");
	{ int inst_ret_val =  1; timer_stop(); return inst_ret_val; }
}
DEF(g) {
    timer_start("int g(void)  [{header.c} {7,1}-{9,1}]");
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
}
int main(void) {
    timer_start("int main(void)  [{header.c} {10,1}-{12,1}]");
	{ int inst_ret_val =  f() + g(); timer_stop(); return inst_ret_val; }
}
