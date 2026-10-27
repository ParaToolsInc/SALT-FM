// The text `return;` in a string before a `return` statement with a value
const char *s(void) {
    timer_start("const char *s(void)  [{return_string.cpp} {2,1}-{2,60}]");
 const char *x = "return;";
	{ const char * inst_ret_val =  x; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_string.cpp} {3,1}-{6,1}]");
	{ int inst_ret_val =  s()[0] == 'r' ? 7 : 1; timer_stop(); return inst_ret_val; }
}
