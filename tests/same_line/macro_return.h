/* A macro with a `return` statement, in a header (#116) */
#define CHECK(x) do { if ((x) > 5) return -1; } while (0)
