// A `;` inside a `return` statement, in a character or string literal,
// in a comment, also over two lines, or in a lambda, does not end the
// statement
#include <string.h>
int is_semi(char c) { return c == ';'; }
int is_sep(const char *s)
{
    return strcmp(s, ";") == 0;
}
int add(int a, int b) { return a /* ; */ + b; }
int sub(int a, int b) { return a // ;
- b; }
int inc(int x) { return [](int v) { return v + 1; }(x); }
int main(void)
{
    return is_semi(';') + is_sep(";") + add(2, 3) + sub(9, 8) + inc(1);
}
