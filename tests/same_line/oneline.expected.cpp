// One-line routines: the start of the body, each `return` statement,
// and the closing brace are all on one line
int kept() {
    timer_start("int kept(void)  [{oneline.cpp} {3,1}-{3,25}]");
	{ int inst_ret_val =  33; timer_stop(); return inst_ret_val; }
}
int keep(int n) {
    timer_start("int keep(int)  [{oneline.cpp} {4,1}-{4,46}]");
 if (n)
	{ int inst_ret_val =  1; timer_stop(); return inst_ret_val; }
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
}
int main()
{
    timer_start("int main(void)  [{oneline.cpp} {5,1}-{8,1}]");
	{ int inst_ret_val =  kept() + keep(0); timer_stop(); return inst_ret_val; }
}
