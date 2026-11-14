// A `return` statement that holds another `return` statement, in a GNU
// statement expression, over two lines or on one, gets no timer, with
// a warning that names the function, and a later function keeps its
// timer (#116)
int lines(int x)
{
    return ({ if (x) return
        7; 1; });
}
int one(int x) { return ({ if (x) return 2; 3; }); }
int later(int x) {
    timer_start("int later(int)  [{return_nested.c} {11,1}-{11,34}]");
	{ int inst_ret_val =  x + 1; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_nested.c} {12,1}-{15,1}]");
	{ int inst_ret_val =  lines(1) + one(0) + later(1); timer_stop(); return inst_ret_val; }
}
