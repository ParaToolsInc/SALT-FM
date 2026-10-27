! An if-return whose condition or return expression would pass column
! 132 on one line, and a `return` split over two lines at column 132.
subroutine long(a, y)
    integer, intent(in) :: a
    integer, intent(out) :: y
    y = 0
    if (a == 1000000 .or. &
        a == 1000001 .or. &
        a == 1000002 .or. &
        a == 1000003 .or. &
        a == 1000004 .or. &
        a == 1000005 .or. &
        a == 1000006 .or. &
        a == 1000007 .or. &
        a == 1000008 .or. &
        a == 1000009 .or. &
        a == 1000010 .or. &
        a == 1000011) return
    y = 1
end subroutine long

subroutine alt(a, *)
    integer, intent(in) :: a
    if (a > 0) return 1 + 0 * (a + a + a + a + a + a + a + a + a + a + &
        a + a + a + a + a + a + a + a + a + a + a + a + a + a + a + a + &
        a + a + a + a + a + a + a + a + a + a + a + a + a + a + a)
end subroutine alt

subroutine cut(a, y)
    integer, intent(in) :: a
    integer, intent(out) :: y
    y = 0
    if (a > 1 .and. a < 1000000)                                                                                                 r&
        &eturn
    y = 1
end subroutine cut

program main
    integer :: y
    call long(5, y)
    print *, 'long', 5, y
    call long(1000003, y)
    print *, 'long', 1000003, y
    call cut(2, y)
    print *, 'cut', 2, y
    call cut(0, y)
    print *, 'cut', 0, y
    call alt(1, *10)
    print *, 'alt not taken'
10  continue
    call alt(0, *20)
    print *, 'alt not taken'
20  continue
end program main
