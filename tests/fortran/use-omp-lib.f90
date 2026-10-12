! `use` without a module nature must find SALT-FM's omp_lib copy in
! the intrinsic module directories
subroutine print_max_threads
  use omp_lib, only: omp_get_max_threads
  implicit none
  print '(i0)', omp_get_max_threads()
end subroutine print_max_threads
