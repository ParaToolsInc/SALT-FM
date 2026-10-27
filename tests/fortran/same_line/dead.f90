! A statement after an unconditional `return` on its line never runs,
! so only its parse and compile are checked.
subroutine dead(y)
    integer, intent(out) :: y
    y = 1
    return; y = 3
end subroutine dead

program main
    integer :: y
    call dead(y)
    print *, 'dead', y
end program main
