#include <cstdio>

template <char C> struct S { int v = C; };

static int f(S<'"'> a, S<'\\'> b, S<'\n'> c) { return a.v + b.v + c.v; }

int main()
{
    if (f({}, {}, {}) != '"' + '\\' + '\n')
        return 1;
    std::puts(__FILE__);
    return 0;
}
