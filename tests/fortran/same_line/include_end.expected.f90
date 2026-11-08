! A procedure in a file included after the last program unit, on lines
! past the last line of this file, gets no timer (#116)
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_include_end.f90} {3,1}-{8,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 1
    call included(y)
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
include 'include_end.inc'
