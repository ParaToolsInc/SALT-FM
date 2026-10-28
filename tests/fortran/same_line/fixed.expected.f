c     The fixed-form cases: statements that share a line with an
c     if-return or a return, labels, continuation lines, a tab-format
c     line, if-returns whose condition or return expression would pass
c     column 72 on one line, also with a character literal across column
c     72, and a `return` split over two lines at column 72, also in a
c     tab-format line.
      subroutine prefix(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &prefix [{fortran_same_line_fixed.f} {7,7}-{12,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      y = 1;
             if (x .gt. 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                           return
      end if
      y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine suffix(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &suffix [{fortran_same_line_fixed.f} {14,7}-{18,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .gt. 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                    return
      end if
                            y = 3 ! after the return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine label_if(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_if [{fortran_same_line_fixed.f} {20,7}-{29,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 10
      y = 1
      go to 20
   10 if (x .lt. -2) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                     return
      end if
      y = 2
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine label_before(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_before [{fortran_same_line_fixed.f} {31,7}-{40,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 10
      y = 5
      go to 20
   10 y = 1;
             if (x .lt. -2) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                            return
      end if
      y = y + 10
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine cont(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &cont [{fortran_same_line_fixed.f} {42,7}-{47,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      y = 6;
             if (x .gt. 0 .and.
     &    x .lt. 10) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                     return
      end if
                             y = 7
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine next_line(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &next_line [{fortran_same_line_fixed.f} {49,7}-{54,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .gt. 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                    return
      end if
      y = 5
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine before_line(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &before_line [{fortran_same_line_fixed.f} {56,7}-{61,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0;
     &
      if (x .gt. 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                    return
      end if
      y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine tab(y)
      integer y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &tab [{fortran_same_line_fixed.f} {63,7}-{67,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
	y=8;
      call TAU_PROFILE_STOP(tauProfileTimer)
	    return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine long(alpha, y)
      integer alpha, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &long [{fortran_same_line_fixed.f} {69,7}-{78,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (alpha .lt. 0) go to 30
      y = 1;
             if (alpha .gt. 0 .and. alpha .lt. 1000000 .and.
     &    alpha .ne. 12345 .and. alpha .ne. 54321) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                   return
      end if
                                                           y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   30 if (alpha .lt. -1 .and. alpha .gt. -1000000 .and.
     &    alpha .ne. -12345 .and. alpha .ne. -54321) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                     return
      end if
                                                             y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine alt(alpha, *, *)
      integer alpha
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alt [{fortran_same_line_fixed.f} {80,7}-{85,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      if (alpha .gt. 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                        return 1 + 0 * (alpha + alpha + alpha + alpha +
     &    alpha + alpha + alpha + alpha + alpha + alpha)
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
      return 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine literal(x, y, s)
      integer x, y
      character*5 s
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &literal [{fortran_same_line_fixed.f} {87,7}-{94,9}]&
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
      subroutine cut(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &cut [{fortran_same_line_fixed.f} {96,7}-{102,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .gt. 0 .and. x .ne. 2 .and. x .ne. 3 .and. x .ne. 444444)
     &then
      call TAU_PROFILE_STOP(tauProfileTimer)
                                                                     ret
     &urn
      end if
      y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine tab_cut(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &tab_cut [{fortran_same_line_fixed.f} {104,7}-{110,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
	if (x .gt. 0 .and. x .ne. 2 .and. x .ne. 3 .and. x .ne. 444444)
     &then
      call TAU_PROFILE_STOP(tauProfileTimer)
	                                                               ret
	1urn
      end if
      y = 1
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      program main
      integer i, y, xs(4)
      data xs /-3, -1, 0, 1/
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_fixed.f} {112,7}-{147,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      do 40 i = 1, 4
      call prefix(xs(i), y)
      print *, 'prefix', xs(i), y
      call suffix(xs(i), y)
      print *, 'suffix', xs(i), y
      call label_if(xs(i), y)
      print *, 'label_if', xs(i), y
      call label_before(xs(i), y)
      print *, 'label_before', xs(i), y
      call cont(xs(i), y)
      print *, 'cont', xs(i), y
      call next_line(xs(i), y)
      print *, 'next_line', xs(i), y
      call before_line(xs(i), y)
      print *, 'before_line', xs(i), y
      call long(xs(i), y)
      print *, 'long', xs(i), y
      call literal(xs(i), y, 'hello')
      print *, 'literal', xs(i), y
      call cut(xs(i), y)
      print *, 'cut', xs(i), y
      call tab_cut(xs(i), y)
      print *, 'tab_cut', xs(i), y
   40 continue
      call tab(y)
      print *, 'tab', y
      do 70 i = 1, 4
      call alt(xs(i), *50, *60)
   50 print *, 'alt 1', xs(i)
      go to 70
   60 print *, 'alt 2', xs(i)
   70 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
