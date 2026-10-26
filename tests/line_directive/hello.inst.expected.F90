#line 1 "hello.f90"
program hello
  implicit none
      integer, save :: tauProfileTimer(2) = [0, 0]
      call timer_start("&
     &HELLO [{hello.f90} {1,1}-{4,3}]&
     &")
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &HELLO [{hello.f90} {1,1}-{4,3}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
#line 3 "hello.f90"
  print *, "Hello world"
      call TAU_PROFILE_STOP(tauProfileTimer)
#line 4 "hello.f90"
      call timer_stop()
end
