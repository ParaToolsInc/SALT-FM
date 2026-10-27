// A one-line routine with a nested block: the text between its two
// `return` statements holds a `}`
int clamp(int v) { if (v > 9) { v = 9; return v; } v = v + 1; return v; }
// The same with the opening brace of the routine on the line before
int cap(int v) {
    if (v > 5) { v = 5; return v; } v = v + 2; return v; }
int main()
{
    return clamp(20) * 10 + clamp(4) + cap(9) + cap(1);
}
