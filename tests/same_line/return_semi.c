// A `;` inside a `return` statement, in a character or string literal
// or in a comment, also over two lines, does not end the statement
#include <string.h>
int is_semi(char c) { return c == ';'; }
int is_sep(const char *s)
{
    return strcmp(s, ";") == 0;
}
int add(int a, int b) { return a /* ; */ + b; }
int sub(int a, int b) { return a // ;
- b; }
int main(void)
{
    return is_semi(';') + is_sep(";") + add(2, 3) + sub(9, 8);
}
