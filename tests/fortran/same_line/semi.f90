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

program main
    integer :: x, y
    do x = 0, 1
        call semi_if(x, y)
        print *, 'semi_if', x, y
    end do
    call semi_return(y)
    print *, 'semi_return', y
end program main
