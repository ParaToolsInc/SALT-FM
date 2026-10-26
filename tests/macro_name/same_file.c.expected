void timer_start(const char *name);
void timer_stop(void);
#define RET int
#define DEF(n) int n(void)
#define ONE 1
RET f(void) {
    timer_start("int f(void)  [{same_file.c} {6,1}-{8,1}]");
	{ int inst_ret_val =  ONE; timer_stop(); return inst_ret_val; }
}
int g(void);
DEF(g) {
    timer_start("int g(void)  [{same_file.c} {10,1}-{12,1}]");
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
}
int main(void) {
    timer_start("int main(void)  [{same_file.c} {13,1}-{15,1}]");
	{ int inst_ret_val =  f() + g(); timer_stop(); return inst_ret_val; }
}
