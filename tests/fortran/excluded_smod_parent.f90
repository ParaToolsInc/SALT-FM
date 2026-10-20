! The parent module of excluded_smod.f90
module smod_m
  implicit none
  interface
    module subroutine smod_work(x)
      real, intent(inout) :: x
    end subroutine
  end interface
end module smod_m
