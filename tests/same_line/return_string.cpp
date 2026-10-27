// The text `return;` in a string before a `return` statement with a value
const char *s(void) { const char *x = "return;"; return x; }
int main(void)
{
    return s()[0] == 'r' ? 7 : 1;
}
