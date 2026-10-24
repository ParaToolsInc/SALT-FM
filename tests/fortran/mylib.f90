! A library module that the Flang driver compiles into the include
! directory of a library install, as for MPI or HDF5
module mylib
  use, intrinsic :: iso_c_binding, only: c_int
  implicit none
  integer(c_int), parameter :: mylib_answer = 42_c_int
contains
  subroutine mylib_hello()
    print '(a, i0)', "Hello from mylib: ", mylib_answer
  end subroutine mylib_hello
end module mylib
