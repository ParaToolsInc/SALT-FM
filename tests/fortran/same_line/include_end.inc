! Flang gives the statements of an included file the lines that they
! have in that file, so the timer calls of this procedure would go on
! lines of include_end.f90. These comment lines put the statements
! below past the last line of include_end.f90, where SALT-FM writes no
! timer calls, so the procedure gets no timer (#116). On a line within
! include_end.f90, the timer calls could be out of line order or
! written in the wrong place.
subroutine included(y)
    integer, intent(inout) :: y
    y = y + 1
end subroutine included
