! The cases of #118: a `return` or an if-return that shares its line
! with other statements. The output must keep them, in order, and stop
! the timer only when the `return` runs.
subroutine prefix(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    y = 1; if (x > 0) return
    y = 2
end subroutine prefix

subroutine suffix(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; y = 3
end subroutine suffix

subroutine cont(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    y = 6; if (x > 0 .and. &
        x < 10) return; y = 7
end subroutine cont

subroutine block(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) then; return; end if
    y = 9
end subroutine block

subroutine plain(y)
    integer, intent(out) :: y
    y = 0
    y = 8; return
end subroutine plain

program main
    integer :: x, y
    do x = 0, 1
        call prefix(x, y)
        print *, 'prefix', x, y
        call suffix(x, y)
        print *, 'suffix', x, y
        call cont(x, y)
        print *, 'cont', x, y
        call block(x, y)
        print *, 'block', x, y
    end do
    call plain(y)
    print *, 'plain', y
end program main
