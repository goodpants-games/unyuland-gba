#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <tonc_math.h>

// x always has to be greater than y
#define UPAIR2U(x, y) ((((x) * (x) + (x)) >> 1) + (y))
#define CEIL_DIV(x, width) (((x) + (width) - 1) / (width))
#define IALIGN(x, width) (((x) + (width) - 1) / (width) * (width))

#define FX_FLOOR(n) ((n) & ~(FIX_ONE - 1))
#define TO_FIXED(n) (FIXED)(FIX_ONE * (n))

INLINE int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

#endif