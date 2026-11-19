! Uses the mpi module that SALT-FM makes from MPICH (#92), with
! keyword arguments, which need its explicit interfaces
program use_mpi
  use mpi
  implicit none
  integer :: ierr, rank
  call mpi_init(ierr)
  call mpi_comm_rank(comm=mpi_comm_world, rank=rank, ierror=ierr)
  print '(a, i0)', "rank ", rank
  call mpi_finalize(ierr)
end program use_mpi
