// A `return;` and a `return` with a `void` value on one line
int count = 0;
void bump(int *n) { *n += 1; }
void step(int *n) { if (*n > 5) return; return bump(n); }
int main()
{
    step(&count);
    step(&count);
    return count;
}
