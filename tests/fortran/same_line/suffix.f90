! An if-return followed by other statements on its line, also with a
! comment, by statements on the next line, or by a comment alone. Each
! procedure runs with the return taken and not taken.
subroutine after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; y = 3
end subroutine after

subroutine after_comment(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; y = 3 ! the statement after the return
end subroutine after_comment

subroutine after_two(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return;  y = 3; y = y + &
        1
end subroutine after_two

subroutine next_line(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; &
        y = 5
end subroutine next_line

subroutine next_line_amp(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; & ! continued
        ! a comment line
        & y = 5
end subroutine next_line_amp

subroutine before_line(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0; &
        if (x > 0) return
    y = 4
end subroutine before_line

subroutine comment(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return ! early exit
    y = 4
end subroutine comment

! The End statement follows a continued if-return on its line, so the
! timer stop of the procedure end goes between them
subroutine end_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 2
    if (x > 0) &
        return; end subroutine end_after

program main
    integer :: x, y
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
end program main
