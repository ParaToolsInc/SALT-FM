! Two returns on one line, each one taken in turn, and neither.
subroutine two_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 5) return; if (x > 0) return
    y = 1
end subroutine two_if

subroutine if_then_plain(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) return; return
end subroutine if_then_plain

subroutine between(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 1; if (x > 5) return; y = 2; if (x > 0) return; y = 3
end subroutine between

program main
    integer :: i, y
    integer, parameter :: xs(3) = [6, 1, 0]
    do i = 1, size(xs)
        call two_if(xs(i), y)
        print *, 'two_if', xs(i), y
        call if_then_plain(xs(i), y)
        print *, 'if_then_plain', xs(i), y
        call between(xs(i), y)
        print *, 'between', xs(i), y
    end do
end program main
