! Flang gives these lines the line numbers that they have in this file,
! so these comment lines put the statements below past the last line of
! include_main.f90, where SALT-FM writes nothing.
! .
! .
! .
! .
! .
! .
program main
    integer :: y
    y = 0
    call work(y)
    print *, y
end program main
