// A `return` statement over two lines, with another `return` after it on
// its last line
int ml(int x) {
    timer_scope t("int ml(int)  [{return_if_lines.cpp} {3,1}-{4,18}]");
 if (x) return
  10; return 20; }
int main(void)
{
    timer_scope t("int main(void)  [{return_if_lines.cpp} {5,1}-{8,1}]");
    return ml(1) + ml(0);
}
