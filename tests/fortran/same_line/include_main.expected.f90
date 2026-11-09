! The main program, included past the last line of this file, cannot
! get its setup code or its timer, so saltfm warns, and `work` keeps its
! timer, as the program may set up TAU elsewhere (#116)
subroutine work(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &work [{fortran_same_line_include_main.f90} {4,1}-{7,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine work
include 'include_main.inc'
