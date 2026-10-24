! Uses a module that has no module file, then the library module in
! mylib.f90
program use_missing_and_mylib
  use missing_module
  use mylib, only: mylib_answer
  implicit none
  print '(i0)', mylib_answer
end program use_missing_and_mylib
