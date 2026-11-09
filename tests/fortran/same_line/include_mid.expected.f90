! A procedure whose if-return comes from an included file gets no
! timer and stays as written, with a warning; the main program keeps its
! timer (#116)
subroutine early(n)
  integer, intent(in) :: n
  include "include_mid.inc"
  print *, "line 7"
  print *, "line 8"
  print *, "line 9"
end subroutine early
program main
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_include_mid.f90} {12,1}-{15,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
  call early(0)
  call early(1)
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
