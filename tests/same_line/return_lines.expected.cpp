// `return` statements over two lines, with the closing brace on their
// last line, and one after a `;` on its first line
int f(void) {
    timer_start("int f(void)  [{return_lines.cpp} {3,1}-{4,6}]");
	{ int inst_ret_val =
  1; timer_stop(); return inst_ret_val; }
}
int g(int a) {
    timer_start("int g(int)  [{return_lines.cpp} {5,1}-{6,6}]");
 a += 1;
	{ int inst_ret_val =  a +
  2; timer_stop(); return inst_ret_val; }
}
int main(void) {
    timer_start("int main(void)  [{return_lines.cpp} {7,1}-{7,37}]");
	{ int inst_ret_val =  f() + g(1); timer_stop(); return inst_ret_val; }
}
