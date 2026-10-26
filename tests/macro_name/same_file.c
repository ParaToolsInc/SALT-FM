void timer_start(const char *name);
void timer_stop(void);
#define RET int
#define DEF(n) int n(void)
#define ONE 1
RET f(void) {
  return ONE;
}
int g(void);
DEF(g) {
  return 2;
}
int main(void) {
  return f() + g();
}
