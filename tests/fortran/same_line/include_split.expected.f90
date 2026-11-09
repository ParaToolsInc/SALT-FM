program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_include_split.f90} {1,1}-{7,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 1
    call included(y)
    call empty()
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
subroutine empty()
end subroutine empty
include 'include_split.inc'
