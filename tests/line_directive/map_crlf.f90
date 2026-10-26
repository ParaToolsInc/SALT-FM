subroutine defs(x)
  logical, intent(in) :: x
  if (x) &
#define TWO \
  1 + \
  1
    return
  print *, TWO
#warning defs after
end subroutine defs
