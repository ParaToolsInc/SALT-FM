! A procedure that includes only declarations keeps its timer, as no
! timer call goes in the included file (#116)
subroutine decl(y)
    integer, intent(inout) :: y
    include 'include_decl.inc'
    y = y + three
    if (y > 100) return
    y = y + 1
end subroutine decl

program main
    integer :: y
    y = 0
    call decl(y)
    print *, y
end program main
