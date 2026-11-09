! A procedure whose `contains` or `return` statement ends in a macro gets
! no timer, with a warning, as the output cannot put a timer stop before
! the macro's text (#116). Before, the stop for a `contains` from a macro
! went after the internal procedures, so the output did not compile.
! Internal procedures decide for themselves. A `;` from a macro on the
! line after a `return` stays as written.
#define CONT contains
#define RET return
#define SEMI ;
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_macro_end.F90} {10,1}-{17,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call outer(y)
    call early(y)
    call late(y)
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
subroutine outer(y)
    integer, intent(inout) :: y
    y = y + 1
    y = y + 2; CONT
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_macro_end.F90} {23,5}-{26,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 10
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine outer
subroutine early(y)
    integer, intent(inout) :: y
    y = y + 100
    if (y > 0) RET
    y = y + 1000
end subroutine early
subroutine late(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &late [{fortran_same_line_macro_end.F90} {36,1}-{42,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = y + 10000
    if (y > 5) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
    SEMI
    y = y + 100000
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine late
