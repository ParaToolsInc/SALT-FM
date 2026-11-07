// A `return` statement over two lines with a `//` comment at the end of
// its first line
int add(int a, int b) { return a // c
+ b; }
int main(void)
{
    return add(2, 4);
}
