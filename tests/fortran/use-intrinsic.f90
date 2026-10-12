program use_intrinsic
  ! Flang 23+ resolves `use, intrinsic` only from intrinsic module paths
  use, intrinsic :: iso_fortran_env, only: output_unit
  implicit none
  write(output_unit, '(a)') "Hello from an intrinsic module"
end program use_intrinsic
