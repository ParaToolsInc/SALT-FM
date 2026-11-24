! Includes mpif.h, which SALT-FM makes from MPICH (#92); without it,
! MPI_COMM_WORLD has no type
program include_mpif
  implicit none
  include 'mpif.h'
  integer :: ierr, rank
  call mpi_init(ierr)
  call mpi_comm_rank(MPI_COMM_WORLD, rank, ierr)
  print '(a, i0)', "rank ", rank
  call mpi_finalize(ierr)
end program include_mpif
