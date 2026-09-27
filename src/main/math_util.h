/*
Copyright (C) 2026  pkhead

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <tonc_math.h>
#include <data/sinelut_bin.h>
#include <platutil.h>

// x always has to be greater than y
#define UPAIR2U(x, y) ((((x) * (x) + (x)) >> 1) + (y))
#define CEIL_DIV(x, width) (((x) + (width) - 1) / (width))
#define IALIGN(x, width) (((x) + (width) - 1) / (width) * (width))

#define FX_FLOOR(n) ((n) & ~(FIX_ONE - 1))
#define TO_FIXED(n) (FIXED)(FIX_ONE * (n))
#define FX(n) TO_FIXED(n)

// sqrt(Bx) / B = (sqrt(x) * sqrt(B)) / B
// B = 256; sqrt(B) = 16
#define FXSQRT(x) (isqrt(x) * 16)

// inclusive clamp (because tonc's is max-exclusive?)
static inline int iclamp(int v, int min, int max)
{
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

static inline int ceil_div(int a, int b)
{
    return (a + b - 1) / b;
}

// unordered pairing function of two unsigned integers
static inline uint upair2u(uint a, uint b)
{
    uint x, y;
    if (a < b) x = b, y = a;
    else       x = a, y = b;
    return ((x * x + x) >> 1) + y;
}

// t is an integer [0, 256]
// [0, 256] => [0, 2PI]
static inline FIXED sine_lut(uint t)
{
    const FIXED *lut = (const FIXED *)sinelut_bin;
    return lut[(t & 127) << 1] * (((t & 255) >= 128) ? -1 : 1);
}

ARM_FUNC
int isqrt(int x);

#endif