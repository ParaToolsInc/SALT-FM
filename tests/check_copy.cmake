# Passes when OUT is SRC with a `#line 1 "<SRC>"` line before it, the
# copy saltfm writes of a source file that the file lists exclude.
#
# Usage: cmake -DSRC=<source> -DOUT=<output> -P check_copy.cmake

file(READ "${SRC}" src)
file(READ "${OUT}" out)
if(NOT out STREQUAL "#line 1 \"${SRC}\"\n${src}")
  message(FATAL_ERROR "${OUT} is not ${SRC} after a #line 1 line:\n${out}")
endif()
