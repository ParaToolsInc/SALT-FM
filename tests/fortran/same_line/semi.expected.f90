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
! continuation line. No line of the output may start with that `;`.
subroutine semi_after(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &semi_after [{fortran_same_line_semi.f90} {19,1}-{27,25}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 3
    if (x > 0) go to 10
    y = 4
   10 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
    return   ! c
        ! c2
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine semi_after
program main
    integer :: x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_semi.f90} {29,1}-{41,16}]&
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
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
