#include <cstdio>

void keep_routine(); // in sif_nomain.c, compiled as C++

int main() {
    keep_routine();
    std::printf("file: %s\n", __FILE__);
    return 0;
}
