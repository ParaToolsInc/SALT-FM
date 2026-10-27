! A `go to` to a labeled `return` and to a labeled End statement, with a
! timer stop text that starts with a directive (directive.yaml).
subroutine label_return(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_return [{fortran_same_line_directive.f90} {3,1}-{10,27}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) go to 20
    y = 1
   20 continue
#ifndef NO_TAU
      call TAU_PROFILE_STOP(tauProfileTimer)
#endif
    return
#ifndef NO_TAU
      call TAU_PROFILE_STOP(tauProfileTimer)
#endif
end subroutine label_return
subroutine label_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_end [{fortran_same_line_directive.f90} {12,1}-{18,27}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) go to 20
    y = 1
   20 continue
#ifndef NO_TAU
      call TAU_PROFILE_STOP(tauProfileTimer)
#endif
   end subroutine label_end
program main
    integer :: i, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_directive.f90} {20,1}-{28,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do i = 0, 1
        call label_return(i, y)
        print *, 'label_return', i, y
        call label_end(i, y)
        print *, 'label_end', i, y
    end do
#ifndef NO_TAU
      call TAU_PROFILE_STOP(tauProfileTimer)
#endif
end program main
