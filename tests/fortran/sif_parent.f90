! Excluding `parent` also excludes its internal procedures `child` and
! `child2`
subroutine parent()
  call child()
  call child2()
contains
  subroutine child()
    print *, "child"
  end subroutine child
  subroutine child2()
    print *, "child2"
  end subroutine child2
end subroutine parent

program sif_parent
  call parent()
end program sif_parent
