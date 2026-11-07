// The text `return` in a name before the `return` statement
int code(void)
{
    int return_code = 3; return return_code;
}
int main(void) { return code(); }
