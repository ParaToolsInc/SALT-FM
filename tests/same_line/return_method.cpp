// With --tau_instrument_inline, a method of a class in a lambda in a
// `return` statement gets its own timer code, which the timer stop of
// the function cannot hold, so the function gets no timer, but the
// method, a later function, and main keep theirs, also where the method
// is on a later line of the statement (#116)
int f(void) { return [] { struct S { int m() { return 1; } }; return S{}.m(); }(); }
int g(void)
{
    return [] {
        struct T { int n() { return 2; } };
        return T{}.n();
    }();
}
int later(void) { return 3; }
int main(void)
{
    return f() + g() + later() == 6 ? 0 : 1;
}
