// A `return` statement in a method of a class in a function returns from
// the method, so it gets none of the timer code of the function, which
// may not compile there, and with --tau_instrument_inline only that of
// the method (#116)
const char *f(void) {
    timer_start("const char *f(void)  [{return_local.cpp} {5,1}-{5,87}]");
 struct S { int m() { return 1; } };
	{ const char * inst_ret_val =  S().m() ? "a" : "b"; timer_stop(); return inst_ret_val; }
}
int g(int x)
{
    timer_start("int g(int)  [{return_local.cpp} {6,1}-{13,1}]");
    struct T
    {
        int n(int y) { return y + 1; }
    };
	{ int inst_ret_val =  T().n(x); timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_local.cpp} {14,1}-{17,1}]");
	{ int inst_ret_val =  f()[0] == 'a' ? g(0) - 1 : 1; timer_stop(); return inst_ret_val; }
}
