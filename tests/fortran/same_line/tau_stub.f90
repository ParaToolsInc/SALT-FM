! Stand-ins for the TAU calls of the default configuration file, so the
! output runs without TAU. Each stop must stop the timer that started
! last, as TAU requires: a stop that runs without its `return`, or a
! missing stop, makes the run fail. A common block holds the timers, as
! a module file would clash among tests that compile this file at once.
block data tau_stub_data
  integer :: depth, timers, started(100)
  common /tau_stub_timers/ depth, timers, started
  data depth, timers /0, 0/
end block data tau_stub_data

subroutine tau_profile_init()
end subroutine tau_profile_init

subroutine tau_profile_set_node(node)
  integer, intent(in) :: node
end subroutine tau_profile_set_node

subroutine tau_profile_timer(timer, name)
  integer, intent(inout) :: timer(2)
  character(len=*), intent(in) :: name
  integer :: depth, timers, started(100)
  common /tau_stub_timers/ depth, timers, started
  if (timer(1) == 0) then
    timers = timers + 1
    timer(1) = timers
  end if
end subroutine tau_profile_timer

subroutine tau_profile_start(timer)
  integer, intent(in) :: timer(2)
  integer :: depth, timers, started(100)
  common /tau_stub_timers/ depth, timers, started
  depth = depth + 1
  started(depth) = timer(1)
end subroutine tau_profile_start

subroutine tau_profile_stop(timer)
  integer, intent(in) :: timer(2)
  integer :: depth, timers, started(100)
  common /tau_stub_timers/ depth, timers, started
  if (depth == 0) then
    print *, 'stop of timer', timer(1), 'with no timer started'
    error stop 1
  end if
  if (started(depth) /= timer(1)) then
    print *, 'stop of timer', timer(1), 'but timer', started(depth), &
        'started last'
    error stop 1
  end if
  depth = depth - 1
end subroutine tau_profile_stop
