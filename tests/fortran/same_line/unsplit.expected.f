c     A main program, and a procedure, whose first executable statement
c     shares its line with a declaration get no timer, but a warning and
c     a comment, which is valid in fixed form too. The other procedures
c     keep their timers.
! SALT-FM could not split line 5 to instrument
! s
      subroutine s(y); integer y; y = 1; end
      subroutine t(y)
      integer y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &t [{fortran_same_line_unsplit_fixed.f} {7,7}-{10,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
c     The comment fits in 72 columns also for a name of 63 characters
      subroutine
     &abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk(y)
! SALT-FM could not split line 15 to instrument
! abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk
      integer y; y = 3
      end
! SALT-FM could not split line 18 to instrument
! MAIN
      program main; integer y; call s(y); print *, y
      call t(y)
      print *, y
      call
     &abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk(y)
      print *, y
      end
