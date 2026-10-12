program use_user_iso_fortran_env
  ! Must resolve to the user module in user-iso-fortran-env.f90
  use iso_fortran_env, only: my_marker
  implicit none
  print '(i0)', my_marker
end program use_user_iso_fortran_env
