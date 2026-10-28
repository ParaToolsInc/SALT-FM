// Two `return` statements on one line of a routine whose class has a
// user-defined copy assignment, so the return values are moved
#include <utility>
struct box
{
    int v;
    box(int x) : v(x) {}
    box(const box &o) : v(o.v) {}
    box &operator=(const box &o) { v = o.v; return *this; }
};
box pick(int n) { if (n) return box(4); return box(5); }
int main()
{
    return pick(1).v * 10 + pick(0).v;
}
