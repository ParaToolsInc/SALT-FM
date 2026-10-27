! An if-return followed by other statements on its line, also with a
! comment, by statements on the next line, or by a comment alone. Each
! procedure runs with the return taken and not taken.
subroutine after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &after [{fortran_same_line_suffix.f90} {4,1}-{9,20}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                       y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine after
subroutine after_comment(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &after_comment [{fortran_same_line_suffix.f90} {11,1}-{16,28}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                       y = 3 ! the statement after the return
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine after_comment
subroutine after_two(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &after_two [{fortran_same_line_suffix.f90} {18,1}-{24,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                        y = 3; y = y + &
        1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine after_two
subroutine next_line(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &next_line [{fortran_same_line_suffix.f90} {26,1}-{32,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
        y = 5
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine next_line
subroutine next_line_amp(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &next_line_amp [{fortran_same_line_suffix.f90} {34,1}-{41,28}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
          y = 5
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine next_line_amp
subroutine before_line(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &before_line [{fortran_same_line_suffix.f90} {43,1}-{49,26}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0; &
        if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                   return
      end if
    y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine before_line
subroutine comment(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &comment [{fortran_same_line_suffix.f90} {51,1}-{57,22}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                      ! early exit
    y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine comment
! The End statement follows a continued if-return on its line, so the
! timer stop of the procedure end goes between them
subroutine end_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &end_after [{fortran_same_line_suffix.f90} {61,1}-{66,40}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 2
    if (x > 0) &
        then
      call TAU_PROFILE_STOP(tauProfileTimer)
        return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                end subroutine end_after
program main
    integer :: x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_suffix.f90} {68,1}-{88,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 1
        call after(x, y)
        print *, 'after', x, y
        call after_comment(x, y)
        print *, 'after_comment', x, y
        call after_two(x, y)
        print *, 'after_two', x, y
        call next_line(x, y)
        print *, 'next_line', x, y
        call next_line_amp(x, y)
        print *, 'next_line_amp', x, y
        call before_line(x, y)
        print *, 'before_line', x, y
        call comment(x, y)
        print *, 'comment', x, y
        call end_after(x, y)
        print *, 'end_after', x, y
    end do
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
