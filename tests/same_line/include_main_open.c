// A main function with its opening brace in an included file gets no
// setup code and no timer, with a warning that says so (#116)
int main(void)
#include "include_open.inc"
    return 0;
}
