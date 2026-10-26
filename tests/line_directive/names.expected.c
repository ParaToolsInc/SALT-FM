int first(void) {
    timer_start("int first(void)  [{names.c} {1,1}-{3,1}]");
	{ int inst_ret_val =  1; timer_stop(); return inst_ret_val; }
}
# 10 "gen.y"
int second(void) {
    timer_start("int second(void)  [{gen.y} {10,1}-{12,1}]");
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
}
int third(void) {
    timer_start("int third(void)  [{gen.y} {20,1}-{22,1}]");
	{ int inst_ret_val =  3; timer_stop(); return inst_ret_val; }
}
#line 1 "a\"b\\c.h"
int fourth(void) {
    timer_start("int fourth(void)  [{a\"b\\c.h} {1,1}-{3,1}]");
	{ int inst_ret_val =  4; timer_stop(); return inst_ret_val; }
}
