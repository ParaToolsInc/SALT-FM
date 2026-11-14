// A `;` inside a `return` statement, in a character or string literal,
// in a comment, also over two lines, or in a lambda, does not end the
// statement
#include <string.h>
int is_semi(char c) {
    timer_start("int is_semi(char)  [{return_semi.cpp} {5,1}-{5,40}]");
	{ int inst_ret_val =  c == ';'; timer_stop(); return inst_ret_val; }
}
int is_sep(const char *s)
{
    timer_start("int is_sep(const char *)  [{return_semi.cpp} {6,1}-{9,1}]");
	{ int inst_ret_val =  strcmp(s, ";") == 0; timer_stop(); return inst_ret_val; }
}
int add(int a, int b) {
    timer_start("int add(int, int)  [{return_semi.cpp} {10,1}-{10,47}]");
	{ int inst_ret_val =  a /* ; */ + b; timer_stop(); return inst_ret_val; }
}
int sub(int a, int b) {
    timer_start("int sub(int, int)  [{return_semi.cpp} {11,1}-{12,6}]");
	{ int inst_ret_val =  a // ;
- b; timer_stop(); return inst_ret_val; }
}
int inc(int x) {
    timer_start("int inc(int)  [{return_semi.cpp} {13,1}-{13,57}]");
	{ int inst_ret_val =  [](int v) { return v + 1; }(x); timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_semi.cpp} {14,1}-{17,1}]");
	{ int inst_ret_val =  is_semi(';') + is_sep(";") + add(2, 3) + sub(9, 8) + inc(1); timer_stop(); return inst_ret_val; }
}
