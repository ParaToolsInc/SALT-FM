! A `go to` to a labeled `return` and to a labeled End statement, with a
! timer stop text that starts with a directive (directive.yaml).
subroutine label_return(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) go to 20
    y = 1
20  return
end subroutine label_return

subroutine label_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    if (x > 0) go to 20
    y = 1
20 end subroutine label_end

program main
    integer :: i, y
    do i = 0, 1
        call label_return(i, y)
        print *, 'label_return', i, y
        call label_end(i, y)
        print *, 'label_end', i, y
    end do
end program main
