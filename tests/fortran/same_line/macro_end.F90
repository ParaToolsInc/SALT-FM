! A procedure whose `contains` or `return` statement ends in a macro gets
! no timer, with a warning, as the output cannot put a timer stop before
! the macro's text (#116). Before, the stop for a `contains` from a macro
! went after the internal procedures, so the output did not compile.
! Internal procedures decide for themselves.
#define CONT contains
#define RET return
program main
    integer :: y
    y = 0
    call outer(y)
    call early(y)
    print *, y
end program main

subroutine outer(y)
    integer, intent(inout) :: y
    y = y + 1
    y = y + 2; CONT
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 10
    end subroutine inner
end subroutine outer

subroutine early(y)
    integer, intent(inout) :: y
    y = y + 100
    if (y > 0) RET
    y = y + 1000
end subroutine early
