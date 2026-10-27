// `return` statements over two lines, with the closing brace on their
// last line, and one after a `;` on its first line
int f(void) { return
  1; }
int g(int a) { a += 1; return a +
  2; }
int main(void) { return f() + g(1); }
