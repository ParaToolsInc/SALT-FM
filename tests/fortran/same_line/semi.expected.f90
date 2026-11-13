! A statement that a continuation line ends with the `;` before an
! if-return or a `return`, with the return taken and not taken.
subroutine semi_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_if [{fortran_same_line_semi.f90} {3,1}-{9,22}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 1 &
      ;
        if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                   return
      end if
    y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_if
subroutine semi_return(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_return [{fortran_same_line_semi.f90} {11,1}-{15,26}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 8 &
      ;
      call TAU_PROFILE_STOP(tauProfileTimer)
        return
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_return
! A labeled `return` whose `;` and a comment, after the `;`, go on in a
! continuation line. The output keeps the `&` and that line as written,
! as no line may start with a `;` but a continuation line.
subroutine semi_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_after [{fortran_same_line_semi.f90} {20,1}-{28,25}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 3
    if (x > 0) go to 10
    y = 4
   10 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
    return & ! c
      ; ! c2
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_after
! The same with no label and a comment that starts with `!$`, after an
! if-return and a `return`. On a line of its own, with -fopenmp, the
! comment would be code, which here does not compile.
subroutine semi_omp(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_omp [{fortran_same_line_semi.f90} {33,1}-{42,23}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 5
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return &
      ; !$ y = 99 +
      end if
    y = 6
      call TAU_PROFILE_STOP(tauProfileTimer)
    return &
      ; !$ y = 98 +
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_omp
! The same after a `contains` and an End statement
subroutine semi_end(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_end [{fortran_same_line_semi.f90} {45,1}-{49,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 7
    call inner(y)
      call TAU_PROFILE_STOP(tauProfileTimer)
contains & ! c
      ; ! c2
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_semi.f90} {51,5}-{54,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 10
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner &
      ; !$ y = 97 +
end subroutine semi_end
! A `;` with no comment after it, after an if-return
subroutine semi_bare(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_bare [{fortran_same_line_semi.f90} {59,1}-{66,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 11
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return &
      ;
      end if
    y = 12
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_bare
program main
    integer :: x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_semi.f90} {68,1}-{90,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
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
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
