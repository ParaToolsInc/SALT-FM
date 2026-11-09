! A procedure whose first executable statement ends in a macro, followed
! by a program unit without macros, gets its timer (#116)
subroutine s(n)
    integer, intent(out) :: n
    character(len=100) :: name
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &s [{fortran_same_line_macro_then.F90} {3,1}-{8,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    name = __FILE__
    n = len_trim(name)
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine s
program main
    integer :: n
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_macro_then.F90} {10,1}-{14,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call s(n)
    print *, n > 0
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
