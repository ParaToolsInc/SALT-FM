// A one-line `void` routine with several statements and no `return`
void set_bit(unsigned *flags, int bit) { unsigned mask = 1u; mask <<= bit; *flags |= mask; }
int main()
{
    unsigned flags = 0;
    set_bit(&flags, 3);
    set_bit(&flags, 1);
    return (int)flags;
}
