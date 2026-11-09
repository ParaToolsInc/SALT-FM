! A procedure whose if-return comes from an included file gets no
! timer and stays as written, with a warning; the main program keeps its
! timer (#116)
subroutine early(n)
  integer, intent(in) :: n
  include "include_mid.inc"
  print *, "line 7"
  print *, "line 8"
  print *, "line 9"
end subroutine early

program main
  call early(0)
  call early(1)
end program main
