#include "math_util.h"

// https://web.archive.org/web/20120306040058/http://medialab.freaknet.org/martin/src/sqrt/sqrt.c
ARM_FUNC
int isqrt(int x)
{
    // Logically, these are unsigned. We need the sign bit to test whether
    // (op - res - one) underflowed.
    int res, one;
    res = 0;

    // "one" starts at the highest power of four <= than the argument.
    one = 1 << 30; // second-to-top bit set
    while (one > x) one >>= 2;

    while (one != 0)
    {
        if (x >= res + one)
        {
            x -= res + one;
            res += one << 1;
        }
        res >>= 1;
        one >>= 2;
    }

    return res;
}