! Hosts with internal procedures whose `return` and End statement share
! a line with other statements. The timer of each internal procedure
! stops at its `return` or End statement, and the timer of each host at
! its `return` or before its `contains`.
subroutine host(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    call inner(x, y)
    if (x > 1) return
    y = y + 10
contains
    subroutine inner(x, y)
        integer, intent(in) :: x
        integer, intent(inout) :: y
        y = y + 1; if (x > 0) return; y = y + 2
        y = y + 4; end subroutine inner
end subroutine host

integer function fhost(x)
    integer, intent(in) :: x
    fhost = finner(x)
    if (x > 1) return
    fhost = fhost + 100
contains
    integer function finner(x)
        integer, intent(in) :: x
        finner = x; if (x > 0) return; finner = -1; end function finner
end function fhost

program main
    integer :: x, y
    integer, external :: fhost
    do x = 0, 2
        call host(x, y)
        print *, 'host', x, y
        print *, 'fhost', x, fhost(x)
        call minner(x)
    end do
contains
    subroutine minner(x)
        integer, intent(in) :: x
        if (x > 1) return; print *, 'minner', x; end subroutine minner
end program main
