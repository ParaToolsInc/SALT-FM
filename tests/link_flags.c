/* saltfm drops the linker flags after `--`, but keeps -D_OPENMP, which
   the build compiles this function with, and -Wlong-long, which warns
   about `long long` with -std=c89 */
#ifdef _OPENMP
long long twice(long long x)
{
    return 2 * x;
}
#endif
/* The test gives this flag as the value of a separate -L, -l, and
   -Xlinker */
#ifdef LINK_FLAG_VALUE
#error "saltfm kept the value of a separate -L, -l, or -Xlinker"
#endif
