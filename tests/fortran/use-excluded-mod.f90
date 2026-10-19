program use_excluded_mod
  ! Must find the module file of excluded_mod.f90
  use excluded_mod, only: excluded_marker
  implicit none
  print '(i0)', excluded_marker
end program use_excluded_mod
