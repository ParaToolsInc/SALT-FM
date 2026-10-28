// A `return` statement over two lines, with another `return` after it on
// its last line
int ml(int x) { if (x) return
  10; return 20; }
int main(void)
{
    return ml(1) + ml(0);
}
