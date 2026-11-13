! Three procedures whose first executable statements end in a macro all
! get their timers, also the last one, after which the file has no
! macro (#116). A macro inside an if-return does not change it.
#define INC(v) v = v + 1
#define ONE 1
program main
    integer :: y
    y = 0
    call a(y)
    call b(y)
    call c(y)
    print *, y
end program main

subroutine a(y)
    integer, intent(inout) :: y
    INC(y)
    if (y > ONE) return
    y = y + 10
end subroutine a

subroutine b(y)
    integer, intent(inout) :: y
    y = y + ONE
end subroutine b

subroutine c(y)
    integer, intent(inout) :: y
    INC(y)
end subroutine c
