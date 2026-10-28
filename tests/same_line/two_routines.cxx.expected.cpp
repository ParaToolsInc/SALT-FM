// Two routines on one line: the closing brace of the first comes before
// the `return` statement of the second
void twice(int *n) {
    timer_scope t("void twice(int *)  [{two_routines.cpp} {3,1}-{3,31}]");
 *n *= 2; } int add(int a, int b) {
    timer_scope t("int add(int, int)  [{two_routines.cpp} {3,33}-{3,71}]");
 return a + b; }
int main()
{
    timer_scope t("int main(void)  [{two_routines.cpp} {4,1}-{9,1}]");
    int n = 3;
    twice(&n);
    return add(n, 1);
}
