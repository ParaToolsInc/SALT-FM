! A procedure whose first executable statement ends in a macro, followed
! by a program unit without macros, gets its timer (#116)
subroutine s(n)
    integer, intent(out) :: n
    character(len=100) :: name
    name = __FILE__
    n = len_trim(name)
end subroutine s

program main
    integer :: n
    call s(n)
    print *, n > 0
end program main
