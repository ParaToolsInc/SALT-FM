/* A `return` statement in a block, with -fblocks, returns from the
   block, so it gets none of the timer code of the function, which may
   not compile there (#116) */
const char *f(int x)
{
    int (^b)(void) = ^{ return 1; };
    return b() + x ? "a" : "b";
}
int main(void)
{
    return f(0)[0] == 'a' ? 0 : 1;
}
