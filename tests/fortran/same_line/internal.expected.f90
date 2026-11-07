! Hosts with internal procedures whose `return` and End statement share
! a line with other statements. The timer of each internal procedure
! stops at its `return` or End statement, and the timer of each host at
! its `return` or before its `contains`.
subroutine host(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &host [{fortran_same_line_internal.f90} {5,1}-{12,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call inner(x, y)
    if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
    y = y + 10
      call TAU_PROFILE_STOP(tauProfileTimer)
contains
    subroutine inner(x, y)
        integer, intent(in) :: x
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_internal.f90} {13,5}-{17,39}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1;
                   if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                              return
      end if
                                      y = y + 2
        y = y + 4;
      call TAU_PROFILE_STOP(tauProfileTimer)
                   end subroutine inner
end subroutine host
integer function fhost(x)
    integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &fhost [{fortran_same_line_internal.f90} {20,1}-{25,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    fhost = finner(x)
    if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
    fhost = fhost + 100
      call TAU_PROFILE_STOP(tauProfileTimer)
contains
    integer function finner(x)
        integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &finner [{fortran_same_line_internal.f90} {26,5}-{28,71}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        finner = x;
                    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                               return
      end if
                                       finner = -1;
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                    end function finner
end function fhost
program main
    integer :: x, y
    integer, external :: fhost
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_internal.f90} {31,1}-{40,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 2
        call host(x, y)
        print *, 'host', x, y
        print *, 'fhost', x, fhost(x)
        call minner(x)
    end do
      call TAU_PROFILE_STOP(tauProfileTimer)
contains
    subroutine minner(x)
        integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &minner [{fortran_same_line_internal.f90} {41,5}-{43,70}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                   return
      end if
                           print *, 'minner', x;
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                 end subroutine minner
end program main
