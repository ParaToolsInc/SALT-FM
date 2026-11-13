! `semi_omp` of semi.f90 with CRLF line ends and a comment after each
! return. Each `!$` comment goes after the comment of its return; a CR
! between them would start a line for gfortran's preprocessor, where,
! with -fopenmp, the `!$` comment would be code, which does not compile.
subroutine semi_crlf(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 5
    if (x > 0) return & ! c
      ; !$ y = 99 +
    y = 6
    return & ! c2
      ; !$ y = 98 +
end subroutine semi_crlf

program main
    integer :: x, y
    do x = 0, 1
        call semi_crlf(x, y)
        print *, 'semi_crlf', x, y
    end do
end program main
