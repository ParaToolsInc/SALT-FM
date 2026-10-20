! A child submodule: must find the module file of submodule smod_s in
! excluded_smod.f90
submodule (smod_m:smod_s) smod_t
  implicit none
contains
  subroutine smod_extra()
    print *, smod_scale
  end subroutine
end submodule smod_t
