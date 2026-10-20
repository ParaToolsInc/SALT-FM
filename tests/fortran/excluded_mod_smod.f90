! A module and its submodule in a source that the file lists exclude;
! the module makes it a module source
module mod_smod_m
  implicit none
  interface
    module subroutine mod_smod_work(x)
      real, intent(inout) :: x
    end subroutine
  end interface
end module mod_smod_m

submodule (mod_smod_m) mod_smod_s
  implicit none
contains
  module subroutine mod_smod_work(x)
    real, intent(inout) :: x
    x = 2.0 * x
  end subroutine
end submodule mod_smod_s
