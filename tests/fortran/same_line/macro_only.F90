! A program unit whose first executable statement ends in a macro, with
! no macro after it in the file, gets its timer (#116)
#define ONE 1
program main
    integer :: x
    x = ONE
    print *, x
end program main
