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

#ifndef AUDIOUTIL_H
#define AUDIOUTIL_H

#include <stdint.h>
#include <limits.h>

#define PI 3.14159265358979323846264338327950288419
#define PI2 (2.0 * 3.14159265358979323846264338327950288419)

static inline int16_t smpconv_f64_s16(double v)
{
    if (v < -1.0) v = -1.0;
    else if (v > 1.0) v = 1.0;
    
    return (int16_t)((INT16_MAX - INT16_MIN) * ((v + 1.0) / 2.0) + INT16_MIN);
}

static inline double smpconv_s16_f64(int16_t v)
{
    return (((double)v - INT16_MIN) / (INT16_MAX - INT16_MIN)) * 2.0 - 1.0;
}

#endif