c     Branches to a labeled `return`: a `go to`, an arithmetic IF, a
c     computed and an assigned GO TO, a labeled alternate return, the
c     alternate return of a call, and `end=` and `err=`, and a `go to`
c     to a labeled End statement, also when the label is alone on its
c     line, and the statement goes on in a continuation line.
      subroutine jump(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 1
      go to 30
   20 return
   30 y = 2
      end

      subroutine arith(x, y)
      integer x, y
      y = 0
      if (x) 20, 30, 30
   20 return
   30 y = 2
      end

      subroutine cgoto(x, y)
      integer x, y
      y = 0
      go to (20, 30) x
   20 return
   30 y = 3
      end

      subroutine agoto(x, y)
      integer x, y, k
      y = 0
      assign 20 to k
      if (x .gt. 0) assign 30 to k
      go to k
   20 return
   30 y = 4
      end

      subroutine alt(x, y, *)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 5
      return
   20 return 1
      end

      subroutine caller(x, y)
      integer x, y
      call alt(x, y, *20)
      y = y + 10
   20 return
      end

      subroutine ioerr(s, y)
      character*(*) s
      integer y
      y = 0
      read (s, *, end=20, err=20) y
      y = y + 7
   20 return
      end

      subroutine label_end(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20 end

      subroutine alone(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20
c     A comment line
     &return
      end

      subroutine alone_tab(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20
	1return
      end

      subroutine alone_end(x, y)
      integer x, y
      y = 0
      if (x .lt. 0) go to 20
      y = 1
   20
     &end

      program main
      integer i, y, xs(2)
      data xs /-1, 1/
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
      end
