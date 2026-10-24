#line 1 "mid.f90"
subroutine first
  print *, "first"
end subroutine first
#line 1 "block.inc"
subroutine middle(n)
  integer, intent(in) :: n
  if (n > 0) &
    return
  print *, "middle"
  return
end subroutine middle
#line 4 "mid.f90"
program mid
  call first()
  call middle(1)
  call third()
contains
  subroutine third()
    print *, "third"
  end subroutine third
end program mid
