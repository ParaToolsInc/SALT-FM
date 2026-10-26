void timer_start(const char *name);
void timer_stop(void);
#line 10 "gen.y"
int first(void) {
#warning first begin
  return 1;
#warning first return
}
#warning first end
#line 1 "block.h"
int second(int n) {
#warning second begin
  if (n)
#line 20
    return 2;
#warning second return after a line directive
  return
    3;
#warning second multiline return
}
#warning second end
#line 30 "gen.y"
int main(void)
#line 40
{
#warning main begin after a line directive
  return first() + second(0);
#warning main return
}
#warning main end
