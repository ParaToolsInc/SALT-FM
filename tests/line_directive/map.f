      subroutine cont(x)
      logical x
      if (x)
   #line 70 "q.inc"
     #return
#warning cont if-return with an indented line directive and a # mark
      end
      subroutine warn(x)
      logical x
      if (x)
#warning warn if-return with a warning line in it
#line 90 "s.inc"
c     a comment line
#warning warn if-return with a warning line after a line directive
     &return
      end
