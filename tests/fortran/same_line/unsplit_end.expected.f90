! A procedure whose statement follows the End statement of the procedure
! before it on its line gets no timer, but a warning and a comment. The
! procedure before it keeps its timer.
subroutine end_line(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &end_line [{fortran_same_line_unsplit_end.f90} {4,1}-{6,34}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 8;
      call TAU_PROFILE_STOP(tauProfileTimer)
           end subroutine end_line
! SALT-FM could not split line 6 to instrument
! after
                                    subroutine after(y); integer y; y = 9; end subroutine after
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_unsplit_end.f90} {8,1}-{14,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call end_line(y)
    print *, 'end_line', y
    call after(y)
    print *, 'after', y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
