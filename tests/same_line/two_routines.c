// Two routines on one line: the closing brace of the first comes before
// the `return` statement of the second. `restrict` is C only, so a C++
// compiler cannot build this file.
void twice(int *restrict n) { *n *= 2; } int add(int a, int b) { return a + b; }
int main(void)
{
    int n = 3;
    twice(&n);
    return add(n, 1);
}
