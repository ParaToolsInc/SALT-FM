! Each procedure body opens with a different OpenMP construct.  With
! -fopenmp the plugin must take the start line from that construct and
! insert the timer start directly above the directive, not inside it.
subroutine omp_parallel
  implicit none
  integer :: n
  !$omp parallel
  n = 1
  !$omp end parallel
end subroutine omp_parallel

subroutine omp_parallel_do
  implicit none
  integer :: i
  !$omp parallel do
  do i = 1, 4
  end do
  !$omp end parallel do
end subroutine omp_parallel_do

subroutine omp_sections
  implicit none
  integer :: n
  !$omp sections
  !$omp section
  n = 1
  !$omp section
  n = 2
  !$omp end sections
end subroutine omp_sections

subroutine omp_barrier
  implicit none
  !$omp barrier
end subroutine omp_barrier

subroutine omp_critical
  implicit none
  integer :: n
  !$omp critical
  n = 1
  !$omp end critical
end subroutine omp_critical

subroutine omp_atomic(n)
  implicit none
  integer, intent(inout) :: n
  !$omp atomic update
  n = n + 1
end subroutine omp_atomic
