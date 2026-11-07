// One-line routines: the start of the body, each `return` statement,
// and the closing brace are all on one line
int kept(void) { return 33; }
int keep(int n) { if (n) return 1; return 2; }
int main(void)
{
    return kept() + keep(0);
}
