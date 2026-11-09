! A main program whose End statement comes from an included file gets
! no timer, but keeps its setup code, which goes before its first
! executable statement, on a line of this file (#116)
subroutine work(y)
    integer, intent(inout) :: y
    y = y + 1
end subroutine work

program main
    integer :: y
    y = 0
    call work(y)
    print *, y
include 'include_main_end.inc'
