#include "pow.h"

#define PRIME POW_LIMIT
#define BIG_X 435679812
#define BIG_Y 100001819

/**
 * @brief Computes the following hash function:
 * f(x) = (X x + Y) % P.
 */
long int pow_hash(long int x) {
  long int result = (x * BIG_X + BIG_Y) % PRIME;
  return result;
}
