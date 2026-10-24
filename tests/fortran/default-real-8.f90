! A generic with only a real(8) specific procedure: a call with a
! default real resolves to it only with -fdefault-real-8
module default_real_8_m
  implicit none
  interface show
    module procedure show_real_8
  end interface show
contains
  subroutine show_real_8(x)
    real(8), intent(in) :: x
    print *, x
  end subroutine show_real_8
end module default_real_8_m

program default_real_8
  use default_real_8_m, only: show
  implicit none
  real :: x
  x = 1.5
  call show(x)
end program default_real_8
