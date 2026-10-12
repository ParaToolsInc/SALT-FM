program use_intrinsic_c_binding
  ! Flang 19-22 need the plugin to add -fintrinsic-modules-path
  ! directories to the intrinsic module directories
  use, intrinsic :: iso_c_binding, only: c_int
  implicit none
  integer(c_int) :: answer
  answer = 42_c_int
  print '(a, i0)', "Hello from iso_c_binding: ", answer
end program use_intrinsic_c_binding
