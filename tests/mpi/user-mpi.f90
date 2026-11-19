! A module of the build named mpi, which must come before the mpi module
! that SALT-FM makes from MPICH (#92)
module mpi
  implicit none
  integer, parameter :: user_mpi_answer = 42
end module mpi
