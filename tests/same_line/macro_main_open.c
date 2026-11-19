// A main function with its opening brace from a macro (#116)
#define BEGIN {
int main(void) BEGIN
    return 0;
}
