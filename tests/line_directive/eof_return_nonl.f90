subroutine s(x)
  logical, intent(in) :: x
  print *, "a"
  if (x) &
    return; end subroutine s