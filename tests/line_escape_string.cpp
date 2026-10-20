#include <cstdio>

template <unsigned N> struct fixed_string {
    char s[N];
    constexpr fixed_string(const char (&a)[N]) { for (unsigned i = 0; i < N; ++i) s[i] = a[i]; }
};
template <fixed_string F> struct T { int v = F.s[1]; };

static int g(T<"a\"b"> t) { return t.v; }

int main()
{
    if (g({}) != '"')
        return 1;
    std::puts(__FILE__);
    return 0;
}
