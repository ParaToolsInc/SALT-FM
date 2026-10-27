// One-line routines: the start of the body, each `return` statement,
// and the closing brace are all on one line
int kept() {
    timer_scope t("int kept(void)  [{oneline.cpp} {3,1}-{3,25}]");
 return 33; }
int keep(int n) {
    timer_scope t("int keep(int)  [{oneline.cpp} {4,1}-{4,46}]");
 if (n) return 1; return 2; }
int main()
{
    timer_scope t("int main(void)  [{oneline.cpp} {5,1}-{8,1}]");
    return kept() + keep(0);
}
