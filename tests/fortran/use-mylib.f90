! Uses the .mod file of the library module in mylib.f90, which the
! Flang driver writes, and the intrinsic modules of Flang
program use_mylib
  use mylib, only: mylib_answer, mylib_hello
  use iso_fortran_env, only: output_unit
  use, intrinsic :: iso_c_binding, only: c_int
  implicit none
  integer(c_int) :: answer
  answer = mylib_answer
  call mylib_hello()
  write(output_unit, '(i0)') answer
end program use_mylib
