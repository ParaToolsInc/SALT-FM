#include "return_end.h"
void timer_stop(void);
int f(void) {
    timer_start("int f(void)  [{return_end.c} {3,1}-{5,1}]");
	{ int inst_ret_val =
  ONE; timer_stop(); return inst_ret_val; }
}
char *p(void) {
    timer_start("char *p(void)  [{return_end.c} {6,1}-{8,1}]");
	{ char * inst_ret_val =  NULL; timer_stop(); return inst_ret_val; }
}
int g(void) {
    timer_start("int g(void)  [{return_end.c} {9,1}-{11,1}]");
	{ int inst_ret_val =  ONE; timer_stop(); return inst_ret_val; }
}
int m(int x, int y) {
    timer_start("int m(int, int)  [{return_end.c} {12,1}-{15,1}]");
	{ int inst_ret_val =  MAX(x,
             y); timer_stop(); return inst_ret_val; }
}
int main(void) {
    timer_start("int main(void)  [{return_end.c} {16,1}-{18,1}]");
	{ int inst_ret_val =  f() + g() + (p() != 0) + m(1, 2); timer_stop(); return inst_ret_val; }
}
