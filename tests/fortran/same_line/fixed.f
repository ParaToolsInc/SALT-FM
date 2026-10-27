c     The fixed-form cases: statements that share a line with an
c     if-return or a return, labels, continuation lines, a tab-format
c     line, if-returns whose condition or return expression would pass
c     column 72 on one line, also with a character literal across column
c     72, and a `return` split over two lines at column 72, also in a
c     tab-format line.
      subroutine prefix(x, y)
      integer x, y
      y = 0
      y = 1; if (x .gt. 0) return
      y = 2
      end

      subroutine suffix(x, y)
      integer x, y
      y = 0
      if (x .gt. 0) return; y = 3 ! after the return
      end

      subroutine label_if(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 10
      y = 1
      go to 20
   10 if (x .lt. -2) return
      y = 2
   20 continue
      end

      subroutine label_before(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 10
      y = 5
      go to 20
   10 y = 1; if (x .lt. -2) return
      y = y + 10
   20 continue
      end

      subroutine cont(x, y)
      integer x, y
      y = 0
      y = 6; if (x .gt. 0 .and.
     &    x .lt. 10) return; y = 7
      end

      subroutine next_line(x, y)
      integer x, y
      y = 0
      if (x .gt. 0) return;
     &y = 5
      end

      subroutine before_line(x, y)
      integer x, y
      y = 0;
     &if (x .gt. 0) return
      y = 4
      end

      subroutine tab(y)
      integer y
      y = 0
	y=8;return
      end

      subroutine long(alpha, y)
      integer alpha, y
      y = 0
      if (alpha .lt. 0) go to 30
      y = 1; if (alpha .gt. 0 .and. alpha .lt. 1000000 .and.
     &    alpha .ne. 12345 .and. alpha .ne. 54321) return; y = 2
      return
   30 if (alpha .lt. -1 .and. alpha .gt. -1000000 .and.
     &    alpha .ne. -12345 .and. alpha .ne. -54321) return; y = 3
      end

      subroutine alt(alpha, *, *)
      integer alpha
      if (alpha .gt. 0) return 1 + 0 * (alpha + alpha + alpha + alpha +
     &    alpha + alpha + alpha + alpha + alpha + alpha)
      return 2
      end

      subroutine literal(x, y, s)
      integer x, y
      character*5 s
      y = 0
      if (x .gt. 0 .and. x .lt. 100000 .and. y .ge. 0 .and. y .lt. 1000
     &    .and. s .eq. 'hello') return
      y = 1
      end

      subroutine cut(x, y)
      integer x, y
      y = 0
      if (x .gt. 0 .and. x .ne. 2 .and. x .ne. 3 .and. x .ne. 444444)ret
     &urn
      y = 1
      end

      subroutine tab_cut(x, y)
      integer x, y
      y = 0
	if (x .gt. 0 .and. x .ne. 2 .and. x .ne. 3 .and. x .ne. 444444)ret
	1urn
      y = 1
      end

      program main
      integer i, y, xs(4)
      data xs /-3, -1, 0, 1/
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
      end
