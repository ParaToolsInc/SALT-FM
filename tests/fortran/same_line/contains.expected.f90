! Hosts whose `contains` statement shares its line with an if-return or
! an assignment, with the return taken and not taken, also with a label,
! a comment, and over continuation lines. The timer of each host stops
! directly before its `contains`, after the statements before it.
module m
    interface
        module subroutine mp_if(x, y)
            integer, intent(in) :: x
            integer, intent(out) :: y
        end subroutine mp_if
    end interface
end module m
submodule (m) sm
contains
    module procedure mp_if
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &mp_if [{fortran_same_line_contains.f90} {16,5}-{19,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = 0
        call inner(y)
        if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
                   return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                           contains
        subroutine inner(y)
            integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {20,9}-{23,28}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
            y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
        end subroutine inner
    end procedure mp_if
end submodule sm
subroutine sub_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &sub_if [{fortran_same_line_contains.f90} {27,1}-{32,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 0
    call inner(y)
    if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                       contains
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {33,5}-{36,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine sub_if
subroutine sub_set(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &sub_set [{fortran_same_line_contains.f90} {39,1}-{44,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = x
    call inner(y)
    y = y + 10;
      call TAU_PROFILE_STOP(tauProfileTimer)
                contains !$ y = 99
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {45,5}-{48,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine sub_set
subroutine sub_label(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &sub_label [{fortran_same_line_contains.f90} {51,1}-{56,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = x
    call inner(y)
    y = y + 20;
   30 continue
      call TAU_PROFILE_STOP(tauProfileTimer)
                   contains
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {57,5}-{60,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine sub_label
subroutine sub_continued(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &sub_continued [{fortran_same_line_contains.f90} {63,1}-{69,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = x
    call inner(y)
    y = y + 30; &
      call TAU_PROFILE_STOP(tauProfileTimer)
    contains
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {70,5}-{73,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine sub_continued
subroutine sub_semi(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &sub_semi [{fortran_same_line_contains.f90} {76,1}-{82,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = x
    call inner(y)
    y = y + 40 &
      ;
      call TAU_PROFILE_STOP(tauProfileTimer)
        contains
    subroutine inner(y)
        integer, intent(inout) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {83,5}-{86,24}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        y = y + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine inner
end subroutine sub_semi
integer function fun_if(x)
    integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &fun_if [{fortran_same_line_contains.f90} {89,1}-{92,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    fun_if = inner(x)
    if (x > 1) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                       contains
    integer function inner(x)
        integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {93,5}-{96,22}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        inner = x + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end function inner
end function fun_if
integer function fun_set(x)
    integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &fun_set [{fortran_same_line_contains.f90} {99,1}-{102,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    fun_set = inner(x)
    fun_set = fun_set + 100;
      call TAU_PROFILE_STOP(tauProfileTimer)
                             contains
    integer function inner(x)
        integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &inner [{fortran_same_line_contains.f90} {103,5}-{106,22}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        inner = x + 1
      call TAU_PROFILE_STOP(tauProfileTimer)
    end function inner
end function fun_set
program main
    use m
    integer :: x, y
    integer, external :: fun_if, fun_set
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_contains.f90} {109,1}-{129,1}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 2
        call mp_if(x, y)
        print *, 'mp_if', x, y
        call sub_if(x, y)
        print *, 'sub_if', x, y
        print *, 'fun_if', x, fun_if(x)
    end do
    call sub_set(1, y)
    print *, 'sub_set', y
    call sub_label(1, y)
    print *, 'sub_label', y
    call sub_continued(1, y)
    print *, 'sub_continued', y
    call sub_semi(1, y)
    print *, 'sub_semi', y
    print *, 'fun_set', fun_set(1)
    call minner();
      call TAU_PROFILE_STOP(tauProfileTimer)
                   contains
    subroutine minner()
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &minner [{fortran_same_line_contains.f90} {130,5}-{132,25}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
        print *, 'minner'
      call TAU_PROFILE_STOP(tauProfileTimer)
    end subroutine minner
end program main
