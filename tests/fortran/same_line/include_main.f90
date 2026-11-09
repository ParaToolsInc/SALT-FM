! The main program, included past the last line of this file, cannot
! get its setup code or its timer, so saltfm warns, and `work` keeps its
! timer, as the program may set up TAU elsewhere (#116)
subroutine work(y)
    integer, intent(inout) :: y
    y = y + 1
end subroutine work
include 'include_main.inc'
