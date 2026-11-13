! A statement that a continuation line ends with the `;` before an
! if-return or a `return`, with the return taken and not taken.
subroutine semi_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 1 &
      ; if (x > 0) return
    y = 2
end subroutine semi_if

subroutine semi_return(y)
    integer, intent(out) :: y
    y = 8 &
      ; return
end subroutine semi_return

! A labeled `return` whose `;` and a comment, after the `;`, go on in a
! continuation line. The output keeps the `&` and that line as written,
! as no line may start with a `;` but a continuation line.
subroutine semi_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 3
    if (x > 0) go to 10
    y = 4
10  return & ! c
      ; ! c2
end subroutine semi_after

! The same with no label and a comment that starts with `!$`, after an
! if-return and a `return`. On a line of its own, with -fopenmp, the
! comment would be code, which here does not compile.
subroutine semi_omp(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 5
    if (x > 0) return &
      ; !$ y = 99 +
    y = 6
    return &
      ; !$ y = 98 +
end subroutine semi_omp

! The same after a `contains` and an End statement
subroutine semi_end(y)
    integer, intent(out) :: y
    y = 7
    call inner(y)
contains & ! c
      ; ! c2
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 10
    end subroutine inner &
      ; !$ y = 97 +
end subroutine semi_end

! A `;` with no comment after it, after an if-return
subroutine semi_bare(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 11
    if (x > 0) return &
      ;
    y = 12
end subroutine semi_bare

program main
    integer :: x, y
    do x = 0, 1
        call semi_if(x, y)
        print *, 'semi_if', x, y
    end do
    call semi_return(y)
    print *, 'semi_return', y
    do x = 0, 1
        call semi_after(x, y)
        print *, 'semi_after', x, y
    end do
    do x = 0, 1
        call semi_omp(x, y)
        print *, 'semi_omp', x, y
    end do
    call semi_end(y)
    print *, 'semi_end', y
    do x = 0, 1
        call semi_bare(x, y)
        print *, 'semi_bare', x, y
    end do
end program main
