// A `;` inside a `return` statement, in a character or string literal
// or in a comment, also over two lines, does not end the statement
#include <string.h>
int is_semi(char c) {
    timer_start("int is_semi(char)  [{return_semi.c} {4,1}-{4,40}]");
	{ int inst_ret_val =  c == ';'; timer_stop(); return inst_ret_val; }
}
int is_sep(const char *s)
{
    timer_start("int is_sep(const char *)  [{return_semi.c} {5,1}-{8,1}]");
	{ int inst_ret_val =  strcmp(s, ";") == 0; timer_stop(); return inst_ret_val; }
}
int add(int a, int b) {
    timer_start("int add(int, int)  [{return_semi.c} {9,1}-{9,47}]");
	{ int inst_ret_val =  a /* ; */ + b; timer_stop(); return inst_ret_val; }
}
int sub(int a, int b) {
    timer_start("int sub(int, int)  [{return_semi.c} {10,1}-{11,6}]");
	{ int inst_ret_val =  a // ;
- b; timer_stop(); return inst_ret_val; }
}
int main(void)
{
    timer_start("int main(void)  [{return_semi.c} {12,1}-{15,1}]");
	{ int inst_ret_val =  is_semi(';') + is_sep(";") + add(2, 3) + sub(9, 8); timer_stop(); return inst_ret_val; }
}
