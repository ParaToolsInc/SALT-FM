c     For a fixed-form line length of 132: an if-return past column 72,
c     and one whose condition, on one line, would have a character
c     literal across column 72.
      subroutine wide(s, y)
      character(len=*) s
      integer y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &wide [{fortran_same_line_wide_fixed.f} {4,7}-{10,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (s .eq. 'abcdefghij' .or. s .eq. 'abcdefghijklmn' .or. s .eq. 'klmnopqrstuvwxyz0123456789') then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                                                                     return
      end if
      y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine literal(x, y, s)
      integer x, y
      character*5 s
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &literal [{fortran_same_line_wide_fixed.f} {12,7}-{19,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .gt. 0 .and. x .lt. 100000 .and. y .ge. 0 .and. y .lt. 1000
     &    .and. s .eq. 'hello') then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                return
      end if
      y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      program main
      integer y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_wide_fixed.f} {21,7}-{31,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      call wide('klmnopqrstuvwxyz0123456789', y)
      print *, 'wide', y
      call wide('k', y)
      print *, 'wide', y
      call literal(1, y, 'hello')
      print *, 'literal', y
      call literal(0, y, 'hello')
      print *, 'literal', y
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
