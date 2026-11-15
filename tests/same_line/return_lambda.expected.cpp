// A `return` statement in the body of a lambda gets no timer stop, also
// where a macro ends it or gives the body, and does not stop the timers
// of its function or of a later function; one in a capture of a lambda
// gets one (#116)
#define NIL nullptr
#define ZERO 0
#define BODY { return 0; }
int f(int x) {
    timer_start("int f(int)  [{return_lambda.cpp} {8,1}-{8,108}]");
 auto l = [](int y) -> int * { if (y) return NIL; return nullptr; };
	{ int inst_ret_val =  l(x) != nullptr; timer_stop(); return inst_ret_val; }
}
int g(int x) {
    timer_start("int g(int)  [{return_lambda.cpp} {9,1}-{9,54}]");
	{ int inst_ret_val =  [](int y) { return ZERO; }(x); timer_stop(); return inst_ret_val; }
}
int k(int x) {
    timer_start("int k(int)  [{return_lambda.cpp} {10,1}-{10,63}]");
 auto l = [] BODY; if (x)
	{ int inst_ret_val =  2; timer_stop(); return inst_ret_val; }
	{ int inst_ret_val =  l(); timer_stop(); return inst_ret_val; }
}
int c(int x) {
    timer_start("int c(int)  [{return_lambda.cpp} {11,1}-{11,84}]");
 auto l = [v = ({ if (x)
	{ int inst_ret_val =  3; timer_stop(); return inst_ret_val; }
 4; })] { return v; };
	{ int inst_ret_val =  l(); timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_lambda.cpp} {12,1}-{15,1}]");
	{ int inst_ret_val =  f(0) + g(0) + k(1) + c(1); timer_stop(); return inst_ret_val; }
}
