! A `go to` to a labeled if-return, to a label on a statement that
! shares its line with an if-return, to a labeled `return`, also after a
! `;` or with the label alone on its line, and to a labeled End
! statement.
subroutine label_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_if [{fortran_same_line_labels.f90} {5,1}-{15,23}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x < 0) go to 10
    y = 1
    go to 20
10  if (x < -2) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                return
      end if
    y = 2
20  continue
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine label_if
subroutine label_before(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_before [{fortran_same_line_labels.f90} {17,1}-{27,27}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x < 0) go to 10
    y = 5
    go to 20
10  y = 1;
           if (x < -2) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                       return
      end if
    y = y + 10
20  continue
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine label_before
subroutine label_return(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_return [{fortran_same_line_labels.f90} {29,1}-{40,27}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x < 0) go to 20
    if (x > 2) go to 31
    y = 1
    if (x > 0) go to 40
    y = 3;
   31 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
              return
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
    return
40  y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine label_return
subroutine label_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_end [{fortran_same_line_labels.f90} {42,1}-{48,27}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x < 0) go to 20
    y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
   end subroutine label_end
subroutine alone(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alone [{fortran_same_line_labels.f90} {50,1}-{58,20}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (x < 0) go to 20
    y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
    return
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine alone
program main
    integer :: i, y
    integer, parameter :: xs(3) = [-3, -1, 1]
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_labels.f90} {60,1}-{77,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do i = 1, size(xs)
        call label_if(xs(i), y)
        print *, 'label_if', xs(i), y
        call label_before(xs(i), y)
        print *, 'label_before', xs(i), y
        call label_end(xs(i), y)
        print *, 'label_end', xs(i), y
        call alone(xs(i), y)
        print *, 'alone', xs(i), y
    end do
    do i = -1, 3
        call label_return(i, y)
        print *, 'label_return', i, y
    end do
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
