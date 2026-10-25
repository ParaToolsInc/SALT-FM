struct timer_scope {
  timer_scope(const char *name);
};
#define CHECK(x) if (!(x)) return -1;
#define END }
#define BEGIN {
int f(int x) {
    timer_scope t("int f(int)  [{cxx_api.cpp} {7,1}-{10,1}]");
  CHECK(x)
  return 0;
}
int h(void) {
    timer_scope t("int h(void)  [{cxx_api.cpp} {11,1}-{13,1}]");
  return 3;
END
int k(void) BEGIN
  return 4;
}
#define CLOSE(x) }
int e(void) {
    timer_scope t("int e(void)  [{cxx_api.cpp} {18,1}-{21,4}]");
  return 5;
CLOSE(
  0)
int main() {
    timer_scope t("int main(void)  [{cxx_api.cpp} {22,1}-{24,1}]");
  return f(1) + h() + k() + e();
}
