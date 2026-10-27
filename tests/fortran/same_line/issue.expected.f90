! The cases of #118: a `return` or an if-return that shares its line
! with other statements. The output must keep them, in order, and stop
! the timer only when the `return` runs.
subroutine prefix(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &prefix [{fortran_same_line_issue.f90} {4,1}-{10,21}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    y = 1;
           if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                      return
      end if
    y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine prefix
subroutine suffix(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &suffix [{fortran_same_line_issue.f90} {12,1}-{17,21}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                       y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine suffix
subroutine cont(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &cont [{fortran_same_line_issue.f90} {19,1}-{25,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    y = 6;
           if (x > 0 .and. &
        x < 10) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                return
      end if
                        y = 7
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine cont
subroutine block(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &block [{fortran_same_line_issue.f90} {27,1}-{33,20}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then;
      call TAU_PROFILE_STOP(tauProfileTimer)
                     return
                             end if
    y = 9
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine block
subroutine plain(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &plain [{fortran_same_line_issue.f90} {35,1}-{39,20}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    y = 8;
      call TAU_PROFILE_STOP(tauProfileTimer)
           return
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine plain
program main
    integer :: x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_issue.f90} {41,1}-{55,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 1
        call prefix(x, y)
        print *, 'prefix', x, y
        call suffix(x, y)
        print *, 'suffix', x, y
        call cont(x, y)
        print *, 'cont', x, y
        call block(x, y)
        print *, 'block', x, y
    end do
    call plain(y)
    print *, 'plain', y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
