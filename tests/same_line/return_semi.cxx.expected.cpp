// A `;` inside a `return` statement, in a character or string literal,
// in a comment, also over two lines, or in a lambda, does not end the
// statement
#include <string.h>
int is_semi(char c) {
    timer_scope t("int is_semi(char)  [{return_semi.cpp} {5,1}-{5,40}]");
 return c == ';'; }
int is_sep(const char *s)
{
    timer_scope t("int is_sep(const char *)  [{return_semi.cpp} {6,1}-{9,1}]");
    return strcmp(s, ";") == 0;
}
int add(int a, int b) {
    timer_scope t("int add(int, int)  [{return_semi.cpp} {10,1}-{10,47}]");
 return a /* ; */ + b; }
int sub(int a, int b) {
    timer_scope t("int sub(int, int)  [{return_semi.cpp} {11,1}-{12,6}]");
 return a // ;
- b; }
int inc(int x) {
    timer_scope t("int inc(int)  [{return_semi.cpp} {13,1}-{13,57}]");
 return [](int v) { return v + 1; }(x); }
int main(void)
{
    timer_scope t("int main(void)  [{return_semi.cpp} {14,1}-{17,1}]");
    return is_semi(';') + is_sep(";") + add(2, 3) + sub(9, 8) + inc(1);
}
