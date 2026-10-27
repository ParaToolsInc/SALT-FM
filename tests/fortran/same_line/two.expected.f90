! Two returns on one line, each one taken in turn, and neither.
subroutine two_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &two_if [{fortran_same_line_two.f90} {2,1}-{8,21}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 5) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
                       if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                  return
      end if
    y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine two_if
subroutine if_then_plain(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &if_then_plain [{fortran_same_line_two.f90} {10,1}-{15,28}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                       return
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine if_then_plain
subroutine between(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &between [{fortran_same_line_two.f90} {17,1}-{21,22}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 1;
           if (x > 5) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                      return
      end if
                              y = 2;
                                     if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                return
      end if
                                                        y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine between
program main
    integer :: i, y
    integer, parameter :: xs(3) = [6, 1, 0]
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_two.f90} {23,1}-{34,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do i = 1, size(xs)
        call two_if(xs(i), y)
        print *, 'two_if', xs(i), y
        call if_then_plain(xs(i), y)
        print *, 'if_then_plain', xs(i), y
        call between(xs(i), y)
        print *, 'between', xs(i), y
    end do
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
