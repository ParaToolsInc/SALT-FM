#include "return_end.h"
void timer_stop(void);
int f(void) { return
  ONE;
}
char *p(void) {
  return NULL;
}
int g(void) {
  return ONE;
}
int m(int x, int y) {
  return MAX(x,
             y);
}
int main(void) {
  return f() + g() + (p() != 0) + m(1, 2);
}
