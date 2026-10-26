#line 100 "orig.f"
      program hello
      implicit none
      call timer_start("&
     &HELLO [{orig.f} {100,7}-{103,9}]&
     &")
      print *, 'Hello world'
      call timer_stop()
      end
