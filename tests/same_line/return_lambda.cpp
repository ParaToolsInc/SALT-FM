// A `return` statement in the body of a lambda gets no timer stop, also
// where a macro ends it or gives the body, and does not stop the timers
// of its function or of a later function; one in a capture of a lambda
// gets one (#116)
#define NIL nullptr
#define ZERO 0
#define BODY { return 0; }
int f(int x) { auto l = [](int y) -> int * { if (y) return NIL; return nullptr; }; return l(x) != nullptr; }
int g(int x) { return [](int y) { return ZERO; }(x); }
int k(int x) { auto l = [] BODY; if (x) return 2; return l(); }
int c(int x) { auto l = [v = ({ if (x) return 3; 4; })] { return v; }; return l(); }
int main(void)
{
    return f(0) + g(0) + k(1) + c(1);
}
