// A `return` statement in a method of a class in a function returns from
// the method, so it gets none of the timer code of the function, which
// may not compile there, and with --tau_instrument_inline only that of
// the method (#116)
const char *f(void) {
    timer_scope t("const char *f(void)  [{return_local.cpp} {5,1}-{5,87}]");
 struct S { int m() { return 1; } }; return S().m() ? "a" : "b"; }
int g(int x)
{
    timer_scope t("int g(int)  [{return_local.cpp} {6,1}-{13,1}]");
    struct T
    {
        int n(int y) { return y + 1; }
    };
    return T().n(x);
}
int main(void)
{
    timer_scope t("int main(void)  [{return_local.cpp} {14,1}-{17,1}]");
    return f()[0] == 'a' ? g(0) - 1 : 1;
}
