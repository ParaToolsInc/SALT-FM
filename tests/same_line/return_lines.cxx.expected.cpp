// `return` statements over two lines, with the closing brace on their
// last line, and one after a `;` on its first line
int f(void) {
    timer_scope t("int f(void)  [{return_lines.cpp} {3,1}-{4,6}]");
 return
  1; }
int g(int a) {
    timer_scope t("int g(int)  [{return_lines.cpp} {5,1}-{6,6}]");
 a += 1; return a +
  2; }
int main(void) {
    timer_scope t("int main(void)  [{return_lines.cpp} {7,1}-{7,37}]");
 return f() + g(1); }
