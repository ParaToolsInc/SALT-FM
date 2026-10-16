int f(void)
{
    return undeclared_name;
}
int main(void)
{
    return f();
}
