! A procedure whose first executable statement shares its line with the
! procedure statement or a declaration gets no timer, but a warning and
! a comment. The other procedures, also its internal ones, keep theirs.
! SALT-FM could not split line 4 to instrument
! one_line
subroutine one_line(y); integer y; y = 1; end subroutine one_line
subroutine decl_line(x, y)
    integer, intent(in) :: x
! SALT-FM could not split line 8 to instrument
! decl_line
    integer, intent(out) :: y; y = 2
    if (x > 0) return
    y = 3
end subroutine decl_line
subroutine outer(y)
! SALT-FM could not split line 14 to instrument
! outer
    integer, intent(out) :: y; call inner(y)
contains
    subroutine inner(y)
        integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_unsplit.f90} {16,5}-{19,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine outer
! No statement comes before a `;` at the start of a line, so this one
! keeps its timer, but not the one after a declaration continued on it
subroutine semi_start(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_start [{fortran_same_line_unsplit.f90} {24,1}-{27,25}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    ; ;y = 5
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_start
subroutine semi_continued(y)
    integer, intent(out) :: y &
! SALT-FM could not split line 31 to instrument
! semi_continued
    ; y = 6
end subroutine semi_continued
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_unsplit.f90} {34,1}-{48,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call one_line(y)
    print *, 'one_line', y
    call decl_line(1, y)
    print *, 'decl_line', 1, y
    call decl_line(0, y)
    print *, 'decl_line', 0, y
    call outer(y)
    print *, 'outer', y
    call semi_start(y)
    print *, 'semi_start', y
    call semi_continued(y)
    print *, 'semi_continued', y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
