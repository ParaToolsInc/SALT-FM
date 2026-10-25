namespace n {
int first() {
#warning first begin
  return 1;
#warning first return
}
}
#line 1 "block.hpp"
namespace m {
int second(int k) {
#warning second begin
  if (k)
#line 20
    return 2;
#warning second return after a line directive
  return 3;
#warning second return
}
#warning second end
}
#line 4 "map.cpp"
int main() {
#warning main begin
  return n::first() + m::second(0);
#warning main return
}
#warning main end
