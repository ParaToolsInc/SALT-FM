! `semi_omp` of semi.f90 with CRLF line ends and a comment after each
! return. Each `!$` comment goes after the comment of its return; a CR
! between them would start a line for gfortran's preprocessor, where,
! with -fopenmp, the `!$` comment would be code, which does not compile.
subroutine semi_crlf(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_crlf [{fortran_same_line_semi_crlf.f90} {5,1}-{14,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 5
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return   ! c !$ y = 99 +
      end if
    y = 6
      call TAU_PROFILE_STOP(tauProfileTimer)
    return   ! c2 !$ y = 98 +
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_crlf
program main
    integer :: x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_semi_crlf.f90} {16,1}-{22,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 1
        call semi_crlf(x, y)
        print *, 'semi_crlf', x, y
    end do
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
