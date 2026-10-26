#line 10 "hello.fypp"
subroutine hello()
  print *, "hello"
#warning hello begin
end subroutine hello
#warning hello end
#line 1 "block.inc"
subroutine middle(n)
  integer, intent(in) :: n
#line 30
  if (n > 0) &
    return
#warning middle begin and if-return after a line directive
  print *, "middle"
  return
#warning middle return
end subroutine middle
#warning middle end
#line 50 "a\b""c.inc"
integer function twice(n)
  integer, intent(in) :: n
  twice = 2 * n
#warning twice begin in a file with a backslash and quotes
end function twice
#warning twice end
subroutine cont(x)
  logical, intent(in) :: x
  if (x) &
#line 70 "q.inc"
    return
#warning cont if-return with a line directive in it
end subroutine cont
subroutine indent(x)
  logical, intent(in) :: x
  if (x) &
    #line 80 "r.inc"
    return
#warning indent if-return with an indented line directive in it
end subroutine indent
subroutine warn(x)
  logical, intent(in) :: x
  if (x) &
#warning warn if-return with a warning line in it
#line 90 "s.inc"
    ! a comment line
#warning warn if-return with a warning line after a line directive
    return
end subroutine warn
subroutine defs(x)
  logical, intent(in) :: x
  if (x) &
#define TWO \
  1 + \
  1
    return
  print *, TWO
#warning defs after a macro of three lines in an if-return
end subroutine defs
#line 20 "hello.fypp"
program main
#line 40
  call hello()
#warning main begin after a line directive
  call middle(1)
contains
#warning main end
  subroutine third()
    print *, "third"
#warning third begin
  end subroutine third
#warning third end
end program main
