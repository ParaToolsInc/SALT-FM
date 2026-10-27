// Two routines on one line: the closing brace of the first comes before
// the `return` statement of the second
void twice(int *n) {
    timer_start("void twice(int *)  [{two_routines.cpp} {3,1}-{3,31}]");
 *n *= 2;
	timer_stop();
} int add(int a, int b) {
    timer_start("int add(int, int)  [{two_routines.cpp} {3,33}-{3,71}]");
	{ int inst_ret_val =  a + b; timer_stop(); return inst_ret_val; }
}
int main()
{
    timer_start("int main(void)  [{two_routines.cpp} {4,1}-{9,1}]");
    int n = 3;
    twice(&n);
	{ int inst_ret_val =  add(n, 1); timer_stop(); return inst_ret_val; }
}
