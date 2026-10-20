#include <cstdio>

static unsigned long long operator""_km(unsigned long long v) { return v * 1000; }

int main()
{
    if (1_km != 1000)
        return 1;
    std::puts(__FILE__);
    return 0;
}
