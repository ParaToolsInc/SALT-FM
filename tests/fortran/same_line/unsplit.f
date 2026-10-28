c     A main program, and a procedure, whose first executable statement
c     shares its line with a declaration get no timer, but a warning and
c     a comment, which is valid in fixed form too. The other procedures
c     keep their timers.
      subroutine s(y); integer y; y = 1; end

      subroutine t(y)
      integer y
      y = 2
      end

c     The comment fits in 72 columns also for a name of 63 characters
      subroutine
     &abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk(y)
      integer y; y = 3
      end

      program main; integer y; call s(y); print *, y
      call t(y)
      print *, y
      call
     &abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk(y)
      print *, y
      end
