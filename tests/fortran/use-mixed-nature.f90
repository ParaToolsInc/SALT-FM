! One file loads both the user module in user-iso-fortran-env.f90 and
! the compiler module of the same name
subroutine print_marker
  use, non_intrinsic :: iso_fortran_env, only: my_marker
  implicit none
  print '(i0)', my_marker
end subroutine print_marker

subroutine print_hello
  use, intrinsic :: iso_fortran_env, only: output_unit
  implicit none
  write(output_unit, '(a)') "Hello from an intrinsic module"
end subroutine print_hello
