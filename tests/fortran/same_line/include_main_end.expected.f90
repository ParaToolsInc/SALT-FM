! A main program whose End statement comes from an included file gets
! no timer, but keeps its setup code, which goes before its first
! executable statement, on a line of this file (#116)
subroutine work(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &work [{fortran_same_line_include_main_end.f90} {4,1}-{7,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine work
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
    y = 0
    call work(y)
    print *, y
include 'include_main_end.inc'
