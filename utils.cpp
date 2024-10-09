#include "utils.h"
#include <iostream>

std::ostream nulls(nullptr);

std::ostream &outs() { return std::cout; }

std::ostream &dbgs() {
#ifndef NDEBUG
  return std::cout;
#else
  return nulls;
#endif
}

std::ostream &errs() { return std::cerr; }