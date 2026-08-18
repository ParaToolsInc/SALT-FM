// Reproducer for https://github.com/ParaToolsInc/salt-fm/issues/64
// (contributed by @giltirn): dosomething_noinst is excluded via the
// leading-wildcard pattern #noinst# in the companion SIF file.
#include <cstdio>

#include<thread>
#include<chrono>

void dosomething_inst(){
  for(int i=0;i<5;i++){
    printf("%d\n",i);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void dosomething_noinst(){
  for(int i=0;i<5;i++){
    printf("%d\n",i);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

int main(void){
  dosomething_inst();
  dosomething_noinst();
  return 0;
}
