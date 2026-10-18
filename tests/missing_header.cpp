#include "Grid/Grid.h"

void scale(RealD factor, ComplexD &z) { z = z * factor; }

void computeOmega(std::vector<ComplexD> &omega_out, const int Ls_out,
                  const std::vector<RealD> &omega_in, const int Ls_in) {}

int count(int n) { return n + 1; }
