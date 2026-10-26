#line 1 "mid.f90"
subroutine first
      call timer_start("&
     &first [{mid.f90} {1,1}-{3,20}]&
     &")
  print *, "first"
      call timer_stop()
end subroutine first
#line 1 "block.inc"
subroutine middle(n)
  integer, intent(in) :: n
      call timer_start("&
     &middle [{block.inc} {1,1}-{7,21}]&
     &")
      if (n > 0) then
      call timer_stop()
        return
      end if
  print *, "middle"
      call timer_stop()
  return
      call timer_stop()
end subroutine middle
#line 4 "mid.f90"
program mid
      call timer_start("&
     &MID [{mid.f90} {4,1}-{8,1}]&
     &")
  call first()
  call middle(1)
  call third()
      call timer_stop()
contains
  subroutine third()
      call timer_start("&
     &third [{mid.f90} {9,3}-{11,22}]&
     &")
    print *, "third"
      call timer_stop()
  end subroutine third
end program mid
