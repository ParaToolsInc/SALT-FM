C     Fixed form that needs the preprocessor: without it, Flang does
C     not define __flang__.
      program fixedcpp
#ifndef __flang__
      this is not Fortran
#endif
      print *, "ok"
      end
