! A return from an `#include` gets its procedure no timer, and the
! internal procedures and the containing procedure of that procedure
! decide for themselves (#116)
program main
    integer :: y
    y = 0
    call outer(y)
    call parent(y)
    print *, y
end program main

! `outer` gets no timer, but its internal procedure `inner` does
subroutine outer(y)
    integer, intent(inout) :: y
    y = y + 1
#include "include_internal.inc"
    call inner(y)
contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 10
    end subroutine inner
end subroutine outer

! `parent` keeps its timer, but its internal procedure `child` gets none
subroutine parent(y)
    integer, intent(inout) :: y
    call child(y)
    y = y + 100
contains
    subroutine child(y)
        integer, intent(inout) :: y
        y = y + 1000
#include "include_internal.inc"
        y = y + 10000
    end subroutine child
end subroutine parent
