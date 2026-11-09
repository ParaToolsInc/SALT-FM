! A return from an `#include` gets its procedure no timer, and the
! internal procedures and the containing procedure of that procedure
! decide for themselves (#116)
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_include_internal.F90} {4,1}-{10,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call outer(y)
    call parent(y)
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
! `outer` gets no timer, but its internal procedure `inner` does
subroutine outer(y)
    integer, intent(inout) :: y
    y = y + 1
#include "include_internal.inc"
    call inner(y)
contains
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_include_internal.F90} {19,5}-{22,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 10
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine outer
! `parent` keeps its timer, but its internal procedure `child` gets none
subroutine parent(y)
    integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &parent [{fortran_same_line_include_internal.F90} {26,1}-{30,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call child(y)
    y = y + 100
      call TAU_PROFILE_STOP(tauProfileTimer)
contains
    subroutine child(y)
        integer, intent(inout) :: y
        y = y + 1000
#include "include_internal.inc"
        y = y + 10000
    end subroutine child
end subroutine parent
