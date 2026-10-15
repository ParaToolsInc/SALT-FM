// Returns that share their line with other code, for the C++ API. A
// routine that a SIF leaves out must keep these lines as written.
int g = 0;

int dropped() { return 11; }

void vdrop(int n) { if (n) return; g = 1; }

int kept() { return 33; }

int main() { vdrop(0); return dropped() + kept() - 44 + g; }
