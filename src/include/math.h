#ifndef MATH_H
#define MATH_H

static unsigned log2_roundup(unsigned n) {
  /* Calculate the floor of log2(n) */
  unsigned l2 = 31 - __builtin_clz(n);

  /* If n == 2^log2(n), floor(n) == n so we can return l2. */
  if (n == 1U<<l2)
    return l2;
  /* else floor(n) != n, so return l2+1 to round up. */
  return l2+1;
}

#endif
