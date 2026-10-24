! A generic with only an integer(8) specific procedure: a call with a
! default integer resolves to it only with -fdefault-integer-8
module default_integer_8_m
  implicit none
  interface show
    module procedure show_integer_8
  end interface show
contains
  subroutine show_integer_8(i)
    integer(8), intent(in) :: i
    print *, i
  end subroutine show_integer_8
end module default_integer_8_m

program default_integer_8
  use default_integer_8_m, only: show
  implicit none
  integer :: i
  i = 42
  call show(i)
end program default_integer_8
