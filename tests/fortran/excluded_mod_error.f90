! A module that Flang cannot parse, in a source that the file lists exclude
module excluded_mod_error
  implicit none
  integer :: i = undeclared_name
end module excluded_mod_error
