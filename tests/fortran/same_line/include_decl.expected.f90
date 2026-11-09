! A procedure that includes only declarations keeps its timer, as no
! timer call goes in the included file (#116)
subroutine decl(y)
    integer, intent(inout) :: y
    include 'include_decl.inc'
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &decl [{fortran_same_line_include_decl.f90} {3,1}-{9,19}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = y + three
    if (y > 100) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                 return
      end if
    y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
end subroutine decl
program main
    integer :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_include_decl.f90} {11,1}-{16,16}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call decl(y)
    print *, y
      call TAU_PROFILE_STOP(tauProfileTimer)
end program main
