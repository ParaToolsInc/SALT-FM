! Uses the mpi_f08 module that SALT-FM makes from MPICH (#92), with
! keyword arguments and without the optional ierror, which need its
! explicit interfaces
program use_mpi_f08
  use mpi_f08
  implicit none
  integer :: rank, ierr
  type(MPI_Comm) :: comm
  call MPI_Init()
  comm = MPI_COMM_WORLD
  call MPI_Comm_rank(comm=comm, rank=rank)
  call MPI_Barrier(comm, ierror=ierr)
  print '(a, i0)', "rank ", rank
  call MPI_Finalize()
end program use_mpi_f08
