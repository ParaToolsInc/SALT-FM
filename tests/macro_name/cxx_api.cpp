struct timer_scope {
  timer_scope(const char *name);
};
#define CHECK(x) if (!(x)) return -1;
#define END }
#define BEGIN {
int f(int x) {
  CHECK(x)
  return 0;
}
int h(void) {
  return 3;
END
int k(void) BEGIN
  return 4;
}
#define CLOSE(x) }
int e(void) {
  return 5;
CLOSE(
  0)
int main() {
  return f(1) + h() + k() + e();
}
