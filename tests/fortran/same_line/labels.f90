! A `go to` to a labeled if-return, to a label on a statement that
! shares its line with an if-return, to a labeled `return`, also after a
! `;` or with the label alone on its line, and to a labeled End
! statement.
subroutine label_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x < 0) go to 10
    y = 1
    go to 20
10  if (x < -2) return
    y = 2
20  continue
end subroutine label_if

subroutine label_before(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x < 0) go to 10
    y = 5
    go to 20
10  y = 1; if (x < -2) return
    y = y + 10
20  continue
end subroutine label_before

subroutine label_return(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x < 0) go to 20
    if (x > 2) go to 31
    y = 1
    if (x > 0) go to 40
    y = 3; 31 return
20  return
40  y = 4
end subroutine label_return

subroutine label_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x < 0) go to 20
    y = 1
20 end subroutine label_end

subroutine alone(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x < 0) go to 20
    y = 1
20  &
    return
end subroutine alone

program main
    integer :: i, y
    integer, parameter :: xs(3) = [-3, -1, 1]
    do i = 1, size(xs)
        call label_if(xs(i), y)
        print *, 'label_if', xs(i), y
        call label_before(xs(i), y)
        print *, 'label_before', xs(i), y
        call label_end(xs(i), y)
        print *, 'label_end', xs(i), y
        call alone(xs(i), y)
        print *, 'alone', xs(i), y
    end do
    do i = -1, 3
        call label_return(i, y)
        print *, 'label_return', i, y
    end do
end program main
