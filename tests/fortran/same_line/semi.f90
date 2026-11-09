! A statement that a continuation line ends with the `;` before an
! if-return or a `return`, with the return taken and not taken.
subroutine semi_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 1 &
      ; if (x > 0) return
    y = 2
end subroutine semi_if

subroutine semi_return(y)
    integer, intent(out) :: y
    y = 8 &
      ; return
end subroutine semi_return

! A labeled `return` whose `;` and a comment, after the `;`, go on in a
! continuation line. No line of the output may start with that `;`.
subroutine semi_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 3
    if (x > 0) go to 10
    y = 4
10  return & ! c
      ; ! c2
end subroutine semi_after

program main
    integer :: x, y
    do x = 0, 1
        call semi_if(x, y)
        print *, 'semi_if', x, y
    end do
    call semi_return(y)
    print *, 'semi_return', y
    do x = 0, 1
        call semi_after(x, y)
        print *, 'semi_after', x, y
    end do
end program main
