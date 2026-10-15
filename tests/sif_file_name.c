#include <stdio.h>

void keep_routine(void); /* in sif_nomain.c */

int main(void) {
    keep_routine();
    printf("file: %s\n", __FILE__);
    return 0;
}
