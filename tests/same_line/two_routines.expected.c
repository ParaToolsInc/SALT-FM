// Two routines on one line: the closing brace of the first comes before
// the `return` statement of the second. `restrict` is C only, so a C++
// compiler cannot build this file.
void twice(int *restrict n) {
    timer_start("void twice(int *__restrict)  [{two_routines.c} {4,1}-{4,40}]");
 *n *= 2;
	timer_stop();
} int add(int a, int b) {
    timer_start("int add(int, int)  [{two_routines.c} {4,42}-{4,80}]");
	{ int inst_ret_val =  a + b; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{two_routines.c} {5,1}-{10,1}]");
    int n = 3;
    twice(&n);
	{ int inst_ret_val =  add(n, 1); timer_stop(); return inst_ret_val; }
}
