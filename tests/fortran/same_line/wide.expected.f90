! An if-return whose condition or return expression would pass column
! 132 on one line, and a `return` split over two lines at column 132.
subroutine long(a, y)
    integer, intent(in) :: a
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &long [{fortran_same_line_wide.f90} {3,1}-{20,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (a == 1000000 .or. &
        a == 1000001 .or. &
        a == 1000002 .or. &
        a == 1000003 .or. &
        a == 1000004 .or. &
        a == 1000005 .or. &
        a == 1000006 .or. &
        a == 1000007 .or. &
        a == 1000008 .or. &
        a == 1000009 .or. &
        a == 1000010 .or. &
        a == 1000011) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                      return
      end if
    y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine long
subroutine alt(a, *)
    integer, intent(in) :: a
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alt [{fortran_same_line_wide.f90} {22,1}-{27,18}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    if (a > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return 1 + 0 * (a + a + a + a + a + a + a + a + a + a + &
        a + a + a + a + a + a + a + a + a + a + a + a + a + a + a + a + &
        a + a + a + a + a + a + a + a + a + a + a + a + a + a + a)
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine alt
subroutine cut(a, y)
    integer, intent(in) :: a
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &cut [{fortran_same_line_wide.f90} {29,1}-{36,18}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    if (a > 1 .and. a < 1000000)                                                                                                 &
     &then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                                                                                                 r&
        &eturn
      end if
    y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine cut
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_wide.f90} {38,1}-{54,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    call long(5, y)
    print *, 'long', 5, y
    call long(1000003, y)
    print *, 'long', 1000003, y
    call cut(2, y)
    print *, 'cut', 2, y
    call cut(0, y)
    print *, 'cut', 0, y
    call alt(1, *10)
    print *, 'alt not taken'
10  continue
    call alt(0, *20)
    print *, 'alt not taken'
20  continue
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
