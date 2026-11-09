! The timer start of this procedure, before its first executable
! statement on line 9 of this file, would go before line 9 of
! include_split.f90, and its timer stop, at its End statement on
! line 11, past the last line of include_split.f90, where SALT-FM
! writes nothing. So the procedure gets no timer, with a warning,
! and the rest of include_split.f90 keeps its timers (#116).
subroutine included(y)
    integer, intent(inout) :: y
    y = y + 1
    y = y + 2
end subroutine included
