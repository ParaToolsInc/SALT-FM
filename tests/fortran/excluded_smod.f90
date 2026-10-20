! A submodule in a source that the file lists exclude; its separate
! module procedure statements do not make it a module
submodule (smod_m) smod_s
  implicit none
  real, parameter :: smod_scale = 2.0
contains
  module subroutine smod_work(x)
    real, intent(inout) :: x
    x = smod_scale * x
  end subroutine
end submodule smod_s
