// Two routines on one line: the closing brace of the first comes before
// the `return` statement of the second
void twice(int *n) { *n *= 2; } int add(int a, int b) { return a + b; }
int main()
{
    int n = 3;
    twice(&n);
    return add(n, 1);
}
