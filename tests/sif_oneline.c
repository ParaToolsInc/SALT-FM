/* Returns that share their line with other code. A routine that a SIF
   leaves out must keep these lines as written, with no timer stop. */
int dropped(void) { return 11; }

int cond_dropped(int n)
{
    if (n == 22) return 22;
    return 0;
}

int kept(void) { return 33; }

int main(void) { return dropped() + cond_dropped(0) + kept() - 44; }
