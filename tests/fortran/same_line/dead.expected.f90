! A statement after an unconditional `return` on its line never runs,
! so only its parse and compile are checked.
subroutine dead(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &dead [{fortran_same_line_dead.f90} {3,1}-{7,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    return
            y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine dead
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_dead.f90} {9,1}-{13,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call dead(y)
    print *, 'dead', y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
