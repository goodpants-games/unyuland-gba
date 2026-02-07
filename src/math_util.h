#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <tonc_math.h>

// x always has to be greater than y
#define UPAIR2U(x, y) ((((x) * (x) + (x)) >> 1) + (y))
#define CEIL_DIV(x, width) (((x) + (width) - 1) / (width))
#define IALIGN(x, width) (((x) + (width) - 1) / (width) * (width))

#define FX_FLOOR(n) ((n) & ~(FIX_ONE - 1))
#define TO_FIXED(n) (FIXED)(FIX_ONE * (n))

static inline int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

// unordered pairing function of two unsigned integers
static inline uint upair2u(uint a, uint b)
{
    uint x, y;
    if (a < b) x = a, y = b;
    else       x = b, y = a;
    return ((x * x + x) >> 1) + y;
}

#endif