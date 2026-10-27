// A one-line routine with a nested block: the text between its two
// `return` statements holds a `}`
int clamp(int v) {
    timer_start("int clamp(int)  [{nested_block.cpp} {3,1}-{3,73}]");
 if (v > 9) { v = 9;
	{ int inst_ret_val =  v; timer_stop(); return inst_ret_val; }
 } v = v + 1;
	{ int inst_ret_val =  v; timer_stop(); return inst_ret_val; }
}
// The same with the opening brace of the routine on the line before
int cap(int v) {
    timer_start("int cap(int)  [{nested_block.cpp} {5,1}-{6,58}]");
    if (v > 5) { v = 5;
	{ int inst_ret_val =  v; timer_stop(); return inst_ret_val; }
 } v = v + 2;
	{ int inst_ret_val =  v; timer_stop(); return inst_ret_val; }
}
int main()
{
    timer_start("int main(void)  [{nested_block.cpp} {7,1}-{10,1}]");
	{ int inst_ret_val =  clamp(20) * 10 + clamp(4) + cap(9) + cap(1); timer_stop(); return inst_ret_val; }
}
