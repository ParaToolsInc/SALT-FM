! An End statement on the line of a `return`, an if-return, or other
! statements, also with a comment. The timer stop of the procedure end
! goes directly before the End statement.
subroutine if_return_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &if_return_end [{fortran_same_line_end_line.f90} {4,1}-{8,51}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 2
    if (x > 0) then
      call TAU_PROFILE_STOP(tauProfileTimer)
               return
      end if
      call TAU_PROFILE_STOP(tauProfileTimer)
                       end subroutine if_return_end
subroutine statement_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &statement_end [{fortran_same_line_end_line.f90} {10,1}-{14,43}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 2
    y = y + x;
      call TAU_PROFILE_STOP(tauProfileTimer)
               end subroutine statement_end
subroutine block_end(x, y)
    integer, intent(in) :: x
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &block_end [{fortran_same_line_end_line.f90} {16,1}-{20,68}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 2
    if (x > 0) then; y = 3;
      call TAU_PROFILE_STOP(tauProfileTimer)
                            return
                                    end if;
      call TAU_PROFILE_STOP(tauProfileTimer)
                                            end subroutine block_end
subroutine return_end(y)
    integer, intent(out) :: y
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &return_end [{fortran_same_line_end_line.f90} {22,1}-{24,44}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    y = 8;
      call TAU_PROFILE_STOP(tauProfileTimer)
           return
      call TAU_PROFILE_STOP(tauProfileTimer)
                   end subroutine return_end
integer function twice(x)
    integer, intent(in) :: x
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &twice [{fortran_same_line_end_line.f90} {26,1}-{28,37}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    twice = 2 * x;
      call TAU_PROFILE_STOP(tauProfileTimer)
                   end function twice
                                      ! the end of twice
program main
    integer :: x, y
    integer, external :: twice
      integer, save :: tauProfileTimer(2) = [0, 0]
      call TAU_PROFILE_INIT()
#ifndef TAU_MPI
      call  TAU_PROFILE_SET_NODE(0)
#endif
      call TAU_PROFILE_TIMER(tauProfileTimer, "&
     &MAIN [{fortran_same_line_end_line.f90} {30,1}-{43,48}]&
     &")
      call TAU_PROFILE_START(tauProfileTimer)
    do x = 0, 1
        call if_return_end(x, y)
        print *, 'if_return_end', x, y
        call statement_end(x, y)
        print *, 'statement_end', x, y
        call block_end(x, y)
        print *, 'block_end', x, y
    end do
    call return_end(y)
    print *, 'return_end', y
    print *, 'twice', twice(4);
      call TAU_PROFILE_STOP(tauProfileTimer)
                                end program main
