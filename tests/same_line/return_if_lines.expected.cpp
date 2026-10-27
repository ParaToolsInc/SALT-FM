// A `return` statement over two lines, with another `return` after it on
// its last line
int ml(int x) {
    timer_start("int ml(int)  [{return_if_lines.cpp} {3,1}-{4,18}]");
 if (x)
	{ int inst_ret_val =
  10; timer_stop(); return inst_ret_val; }
	{ int inst_ret_val =  20; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_if_lines.cpp} {5,1}-{8,1}]");
	{ int inst_ret_val =  ml(1) + ml(0); timer_stop(); return inst_ret_val; }
}
