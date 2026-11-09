! A program unit whose first executable statement ends in a macro, with
! no macro after it in the file, gets its timer (#116)
#define ONE 1
program main
    integer :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_macro_only.F90} {4,1}-{8,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    x = ONE
    print *, x
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
