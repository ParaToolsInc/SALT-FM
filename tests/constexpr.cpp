// A constexpr or consteval function, method, or constructor gets no
// timer, as timer calls cannot run at compile time (#147). Nor does a
// lambda, whose function Clang makes constexpr, but the function that
// holds the lambda or a constexpr method in its `return` statement
// keeps its timer.
constexpr int square(int n)
{
    return n * n;
}

consteval int cube(int n)
{
    return n * n * n;
}

struct Point
{
    constexpr Point(int x) : x(x)
    {
    }
    constexpr int twice() const
    {
        return 2 * x;
    }
    int x;
};

static_assert(square(3) == 9);
static_assert(Point(2).twice() == 4);

int run(int n)
{
    auto add = [](int a, int b) { return a + b; };
    return add(square(n), cube(2));
}

int local(void)
{
    return [] { struct S { constexpr int m() { return 1; } }; return S{}.m(); }();
}

int main(void)
{
    return run(1) + local() == 10 ? 0 : 1;
}
