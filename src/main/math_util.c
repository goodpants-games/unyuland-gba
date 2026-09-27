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