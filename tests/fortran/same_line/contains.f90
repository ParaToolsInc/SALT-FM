! Hosts whose `contains` statement shares its line with an if-return or
! an assignment, with the return taken and not taken, also with a label
! and over continuation lines. The timer of each host stops directly
! before its `contains`, after the statements before it on its line.
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
        y = 0
        call inner(y)
        if (x > 1) return; contains
        subroutine inner(y)
            integer, intent(inout) :: y
            y = y + 1
        end subroutine inner
    end procedure mp_if
end submodule sm

subroutine sub_if(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = 0
    call inner(y)
    if (x > 1) return; contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 1
    end subroutine inner
end subroutine sub_if

subroutine sub_set(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = x
    call inner(y)
    y = y + 10; contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 1
    end subroutine inner
end subroutine sub_set

subroutine sub_label(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = x
    call inner(y)
    y = y + 20; 30 contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 1
    end subroutine inner
end subroutine sub_label

subroutine sub_continued(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = x
    call inner(y)
    y = y + 30; &
    contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 1
    end subroutine inner
end subroutine sub_continued

subroutine sub_semi(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
    y = x
    call inner(y)
    y = y + 40 &
      ; contains
    subroutine inner(y)
        integer, intent(inout) :: y
        y = y + 1
    end subroutine inner
end subroutine sub_semi

integer function fun_if(x)
    integer, intent(in) :: x
    fun_if = inner(x)
    if (x > 1) return; contains
    integer function inner(x)
        integer, intent(in) :: x
        inner = x + 1
    end function inner
end function fun_if

integer function fun_set(x)
    integer, intent(in) :: x
    fun_set = inner(x)
    fun_set = fun_set + 100; contains
    integer function inner(x)
        integer, intent(in) :: x
        inner = x + 1
    end function inner
end function fun_set

program main
    use m
    integer :: x, y
    integer, external :: fun_if, fun_set
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
    call minner(); contains
    subroutine minner()
        print *, 'minner'
    end subroutine minner
end program main
