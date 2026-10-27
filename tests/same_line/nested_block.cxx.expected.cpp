// A one-line routine with a nested block: the text between its two
// `return` statements holds a `}`
int clamp(int v) {
    timer_scope t("int clamp(int)  [{nested_block.cpp} {3,1}-{3,73}]");
 if (v > 9) { v = 9; return v; } v = v + 1; return v; }
// The same with the opening brace of the routine on the line before
int cap(int v) {
    timer_scope t("int cap(int)  [{nested_block.cpp} {5,1}-{6,58}]");
    if (v > 5) { v = 5; return v; } v = v + 2; return v; }
int main()
{
    timer_scope t("int main(void)  [{nested_block.cpp} {7,1}-{10,1}]");
    return clamp(20) * 10 + clamp(4) + cap(9) + cap(1);
}
