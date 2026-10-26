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
      call timer_start("&
     &sep [{sub.inc} {22,3}-{24,19}]&
     &")
    print *, n
      call timer_stop()
  end procedure sep
end submodule line_directive_s
#line 30 "fun.inc"
integer function twice(n)
  integer, intent(in) :: n
      call timer_start("&
     &twice [{fun.inc} {30,1}-{33,18}]&
     &")
  twice = 2 * n
      call timer_stop()
end function twice
