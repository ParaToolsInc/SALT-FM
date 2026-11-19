! Uses the mpi module of the build in user-mpi.f90, not the one that
! SALT-FM makes from MPICH, which has no user_mpi_answer (#92)
program use_user_mpi
  use mpi, only: user_mpi_answer
  implicit none
  print '(i0)', user_mpi_answer
end program use_user_mpi
