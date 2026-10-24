#line 10 "mod.inc"
module line_directive_m
  interface
    module subroutine sep(n)
      integer, intent(in) :: n
    end subroutine sep
  end interface
end module line_directive_m
#line 20 "sub.inc"
submodule (line_directive_m) line_directive_s
contains
  module procedure sep
    print *, n
  end procedure sep
end submodule line_directive_s
#line 30 "fun.inc"
integer function twice(n)
  integer, intent(in) :: n
  twice = 2 * n
end function twice
