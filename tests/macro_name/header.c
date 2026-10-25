#include "header.h"
void timer_start(const char *name);
void timer_stop(void);
RET f(void) {
  return 1;
}
DEF(g) {
  return 2;
}
int main(void) {
  return f() + g();
}
