// The text `return;` in a string before a `return` statement with a value
const char *s(void) {
    timer_scope t("const char *s(void)  [{return_string.cpp} {2,1}-{2,60}]");
 const char *x = "return;"; return x; }
int main(void)
{
    timer_scope t("int main(void)  [{return_string.cpp} {3,1}-{6,1}]");
    return s()[0] == 'r' ? 7 : 1;
}
