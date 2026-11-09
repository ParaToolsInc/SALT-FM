! Flang gives these lines the line numbers that they have in this file,
! so the timer calls of the if-return below, also the first executable
! statement of its procedure, would go at line 8 of include_mid.f90:
! the timer declarations after an executable statement, which does not
! compile, and the if-return in place of the text of that line (#116).
! .
! .
  if (n > 0) return
