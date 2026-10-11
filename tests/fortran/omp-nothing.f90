! The procedure body opens with an OpenMP utility directive (Flang 20+).
! With -fopenmp the plugin must insert the timer start directly above it.
subroutine omp_nothing
  implicit none
  !$omp nothing
end subroutine omp_nothing
