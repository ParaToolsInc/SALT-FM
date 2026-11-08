! A procedure whose statement follows the End statement of the procedure
! before it on its line gets no timer, but a warning and a comment. The
! procedure before it keeps its timer.
subroutine end_line(y)
    integer, intent(out) :: y
    y = 8; end subroutine end_line; subroutine after(y); integer y; y = 9; end subroutine after

program main
    integer :: y
    call end_line(y)
    print *, 'end_line', y
    call after(y)
    print *, 'after', y
end program main
