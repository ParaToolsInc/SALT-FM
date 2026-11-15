// A `return` statement in the body of a lambda gets no timer stop, also
// where a macro ends it or gives the body, and does not stop the timers
// of its function or of a later function; one in a capture of a lambda
// gets one (#116)
#define NIL nullptr
#define ZERO 0
#define BODY { return 0; }
int f(int x) {
    timer_scope t("int f(int)  [{return_lambda.cpp} {8,1}-{8,108}]");
 auto l = [](int y) -> int * { if (y) return NIL; return nullptr; }; return l(x) != nullptr; }
int g(int x) {
    timer_scope t("int g(int)  [{return_lambda.cpp} {9,1}-{9,54}]");
 return [](int y) { return ZERO; }(x); }
int k(int x) {
    timer_scope t("int k(int)  [{return_lambda.cpp} {10,1}-{10,63}]");
 auto l = [] BODY; if (x) return 2; return l(); }
int c(int x) {
    timer_scope t("int c(int)  [{return_lambda.cpp} {11,1}-{11,84}]");
 auto l = [v = ({ if (x) return 3; 4; })] { return v; }; return l(); }
int main(void)
{
    timer_scope t("int main(void)  [{return_lambda.cpp} {12,1}-{15,1}]");
    return f(0) + g(0) + k(1) + c(1);
}
