! An End statement on the line of a `return`, an if-return, or other
! statements, also with a comment. The timer stop of the procedure end
! goes directly before the End statement.
subroutine if_return_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 2
    if (x > 0) return; end subroutine if_return_end

subroutine statement_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 2
    y = y + x; end subroutine statement_end

subroutine block_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 2
    if (x > 0) then; y = 3; return; end if; end subroutine block_end

subroutine return_end(y)
    integer, intent(out) :: y
    y = 8; return; end subroutine return_end

integer function twice(x)
    integer, intent(in) :: x
    twice = 2 * x; end function twice ! the end of twice

program main
    integer :: x, y
    integer, external :: twice
    do x = 0, 1
        call if_return_end(x, y)
        print *, 'if_return_end', x, y
        call statement_end(x, y)
        print *, 'statement_end', x, y
        call block_end(x, y)
        print *, 'block_end', x, y
    end do
    call return_end(y)
    print *, 'return_end', y
    print *, 'twice', twice(4); end program main
