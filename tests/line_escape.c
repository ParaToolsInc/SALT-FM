#include <stdio.h>

static const char *file(void) { return __FILE__; }

int main(void)
{
    puts(file());
    return 0;
}
