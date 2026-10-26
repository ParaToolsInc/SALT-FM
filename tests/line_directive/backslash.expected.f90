#line 1 "/x/a\b/t.f90"
subroutine s()
      call timer_start("&
     &s [{/x/a\b/t.f90} {1,1}-{3,16}]&
     &")
  print *, "s"
      call timer_stop()
end subroutine s
#line 1 "/x/a\\b/u.f90"
subroutine u()
      call timer_start("&
     &u [{/x/a\\b/u.f90} {1,1}-{3,16}]&
     &")
  print *, "u"
      call timer_stop()
end subroutine u
