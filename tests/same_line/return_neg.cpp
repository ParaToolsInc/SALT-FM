// A `return` statement over two lines with a `-` before a `-` on the
// next line, which must stay apart. The code after it on that line
// must keep its line number.
int neg(int x) { if (x > 5) return -
-x; int line = __LINE__; return line; }
int main()
{
    return neg(9) * 10 + neg(3);
}
