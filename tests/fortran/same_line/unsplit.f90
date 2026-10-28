! A procedure whose first executable statement shares its line with the
! procedure statement or a declaration gets no timer, but a warning and
! a comment. The other procedures, also its internal ones, keep theirs.
subroutine one_line(y); integer y; y = 1; end subroutine one_line

subroutine decl_line(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y; y = 2
    if (x > 0) return
    y = 3
end subroutine decl_line

subroutine outer(y)
    integer, intent(out) :: y; call inner(y)
contains
    subroutine inner(y)
        integer, intent(out) :: y
        y = 4
    end subroutine inner
end subroutine outer

! No statement comes before a `;` at the start of a line, so this one
! keeps its timer, but not the one after a declaration continued on it
subroutine semi_start(y)
    integer, intent(out) :: y
    ; ;y = 5
end subroutine semi_start

subroutine semi_continued(y)
    integer, intent(out) :: y &
    ; y = 6
end subroutine semi_continued

program main
    integer :: y
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
end program main
