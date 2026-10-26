void timer_start(const char *name);
void timer_stop(void);
#define BODY { return 1; }
#define RETURN2 return 2;
#define END }
#define BEGIN {
int f(void);
int f(void) BODY
int g(void);
int g(void) {
  RETURN2
}
int h(void) {
  return 3;
END
int k(void) BEGIN
  return 4;
}
int main(void) {
  return f() + g() + h() + k();
}
