! Three procedures whose first executable statements end in a macro all
! get their timers, also the last one, after which the file has no
! macro (#116). A macro inside an if-return does not change it.
#define INC(v) v = v + 1
#define ONE 1
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_macro_three.F90} {6,1}-{13,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call a(y)
    call b(y)
    call c(y)
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
subroutine a(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &a [{fortran_same_line_macro_three.F90} {15,1}-{20,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    INC(y)
    if (y > ONE) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                 return
      end if
    y = y + 10
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine a
subroutine b(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &b [{fortran_same_line_macro_three.F90} {22,1}-{25,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = y + ONE
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine b
subroutine c(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &c [{fortran_same_line_macro_three.F90} {27,1}-{30,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    INC(y)
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine c
