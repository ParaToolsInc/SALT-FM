/* C, not C++: `new` is a keyword in C++ (#142). Clang warns about
   `#pragma once` in a source, not here */
#pragma once
int twice(int new)
{
    return 2 * new;
}
