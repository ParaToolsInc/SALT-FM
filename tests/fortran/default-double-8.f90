! A generic with only a real(8) specific procedure: with
! -fdefault-real-8, a call with a double precision actual resolves to it
! only with -fdefault-double-8 too
module default_double_8_m
  implicit none
  interface show
    module procedure show_real_8
  end interface show
contains
  subroutine show_real_8(x)
    real(8), intent(in) :: x
    print *, x
  end subroutine show_real_8
end module default_double_8_m

program default_double_8
  use default_double_8_m, only: show
  implicit none
  double precision :: d
  d = 1.5d0
  call show(d)
end program default_double_8
