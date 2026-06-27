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