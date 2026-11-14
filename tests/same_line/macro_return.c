// A function whose `return` statement ends with a `;` from a macro, or
// that calls a macro of a header with a `return` statement, gets no
// timer, with a warning that names it, and a later function keeps its
// timer (#116)
#include "macro_return.h"
#define ZERO 0;
int zero(void) {
  return ZERO
}
int check(int n)
{
    CHECK(n);
    return n;
}
int later(int n) { return n + 1; }
int main(void)
{
    return zero() + check(2) + later(3);
}
