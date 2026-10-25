int first(void) {
    timer_start("int first(void)  [{mid.c} {1,1}-{3,1}]");
	{ int inst_ret_val =  1; timer_stop(); return inst_ret_val; }
}
#line 1 "block.h"
int second(void) {
    timer_start("int second(void)  [{block.h} {1,1}-{3,1}]");
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
}
#line 4 "mid.c"
int main(void) {
    timer_start("int main(void)  [{mid.c} {4,1}-{6,1}]");
	{ int inst_ret_val =  first() + second(); timer_stop(); return inst_ret_val; }
}
