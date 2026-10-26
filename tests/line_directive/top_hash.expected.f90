# 100 "orig.f90"
program hello
  implicit none
      call timer_start("&
     &HELLO [{orig.f90} {100,1}-{103,3}]&
     &")
  print *, "Hello world"
      call timer_stop()
end
