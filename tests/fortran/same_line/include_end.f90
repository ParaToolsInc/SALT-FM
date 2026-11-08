! A procedure in a file included after the last program unit, on lines
! past the last line of this file, gets no timer (#116)
program main
    integer :: y
    y = 1
    call included(y)
    print *, y
end program main
include 'include_end.inc'
