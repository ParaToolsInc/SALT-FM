c     Branches to a labeled `return`: a `go to`, an arithmetic IF, a
c     computed and an assigned GO TO, a labeled alternate return, the
c     alternate return of a call, and `end=` and `err=`, and a `go to`
c     to a labeled End statement, also when the label is alone on its
c     line, and the statement goes on in a continuation line.
      subroutine jump(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &jump [{fortran_same_line_labels_fixed.f} {6,7}-{14,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 1
      go to 30
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   30 y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine arith(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &arith [{fortran_same_line_labels_fixed.f} {16,7}-{22,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x) 20, 30, 30
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   30 y = 2
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine cgoto(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &cgoto [{fortran_same_line_labels_fixed.f} {24,7}-{30,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      go to (20, 30) x
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   30 y = 3
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine agoto(x, y)
      integer x, y, k
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &agoto [{fortran_same_line_labels_fixed.f} {32,7}-{40,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      assign 20 to k
      if (x .gt. 0) assign 30 to k
      go to k
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   30 y = 4
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine alt(x, y, *)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alt [{fortran_same_line_labels_fixed.f} {42,7}-{49,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 5
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return 1
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine caller(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &caller [{fortran_same_line_labels_fixed.f} {51,7}-{56,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      call alt(x, y, *20)
      y = y + 10
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine ioerr(s, y)
      character*(*) s
      integer y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &ioerr [{fortran_same_line_labels_fixed.f} {58,7}-{65,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      read (s, *, end=20, err=20) y
      y = y + 7
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine label_end(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &label_end [{fortran_same_line_labels_fixed.f} {67,7}-{72,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine alone(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alone [{fortran_same_line_labels_fixed.f} {74,7}-{82,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine alone_tab(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alone_tab [{fortran_same_line_labels_fixed.f} {84,7}-{91,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
	 return
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      subroutine alone_end(x, y)
      integer x, y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &alone_end [{fortran_same_line_labels_fixed.f} {93,7}-{98,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
      program main
      integer i, y, xs(2)
      data xs /-1, 1/
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_labels_fixed.f} {101,7}-{130,9}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
      do 10 i = 1, 2
      call jump(xs(i), y)
      print *, 'jump', xs(i), y
      call arith(xs(i), y)
      print *, 'arith', xs(i), y
      call cgoto(i, y)
      print *, 'cgoto', i, y
      call agoto(xs(i), y)
      print *, 'agoto', xs(i), y
      call caller(xs(i), y)
      print *, 'caller', xs(i), y
      call label_end(xs(i), y)
      print *, 'label_end', xs(i), y
      call alone(xs(i), y)
      print *, 'alone', xs(i), y
      call alone_tab(xs(i), y)
      print *, 'alone_tab', xs(i), y
      call alone_end(xs(i), y)
      print *, 'alone_end', xs(i), y
   10 continue
      call ioerr('', y)
      print *, 'ioerr', y
      call ioerr('x', y)
      print *, 'ioerr', y
      call ioerr('3', y)
      print *, 'ioerr', y
      call TAU_PROFILE_STOP(tauProfileTimer)
      end
