namespace n {
int first() {
  return 1;
}
}
#line 1 "block.hpp"
namespace m {
int second() {
  return 2;
}
}
#line 4 "mid.cpp"
int main() {
  return n::first() + m::second();
}
