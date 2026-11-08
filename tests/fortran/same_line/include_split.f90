program main
    integer :: y
    y = 1
    call included(y)
    call empty()
    print *, y
end program main
subroutine empty()
end subroutine empty
include 'include_split.inc'
