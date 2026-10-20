! A source that the file lists exclude, with an INCLUDE line and no
! module statement; Flang cannot find the included file
subroutine excluded_include_error
  include 'salt_missing_include.h'
end subroutine excluded_include_error
