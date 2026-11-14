// A `return` statement that holds another `return` statement, in a GNU
// statement expression, over two lines or on one, gets no timer, with
// a warning that names the function, and a later function keeps its
// timer (#116)
int lines(int x)
{
    return ({ if (x) return
        7; 1; });
}
int one(int x) { return ({ if (x) return 2; 3; }); }
int later(int x) { return x + 1; }
int main(void)
{
    return lines(1) + one(0) + later(1);
}
