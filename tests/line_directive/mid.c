int first(void) {
  return 1;
}
#line 1 "block.h"
int second(void) {
  return 2;
}
#line 4 "mid.c"
int main(void) {
  return first() + second();
}
