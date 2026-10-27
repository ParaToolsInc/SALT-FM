c     For a fixed-form line length of 132: an if-return past column 72,
c     and one whose condition, on one line, would have a character
c     literal across column 72.
      subroutine wide(s, y)
      character(len=*) s
      integer y
      y = 0
      if (s .eq. 'abcdefghij' .or. s .eq. 'abcdefghijklmn' .or. s .eq. 'klmnopqrstuvwxyz0123456789') return
      y = 1
      end

      subroutine literal(x, y, s)
      integer x, y
      character*5 s
      y = 0
      if (x .gt. 0 .and. x .lt. 100000 .and. y .ge. 0 .and. y .lt. 1000
     &    .and. s .eq. 'hello') return
      y = 1
      end

      program main
      integer y
      call wide('klmnopqrstuvwxyz0123456789', y)
      print *, 'wide', y
      call wide('k', y)
      print *, 'wide', y
      call literal(1, y, 'hello')
      print *, 'literal', y
      call literal(0, y, 'hello')
      print *, 'literal', y
      end
