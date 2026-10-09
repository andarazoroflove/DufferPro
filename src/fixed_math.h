#ifndef FIXED_MATH_H
#define FIXED_MATH_H

#include <stdint.h>
#include <stdbool.h>

/*
 * 16.16 Deterministic Fixed-Point Math Library
 * Designed for cross-platform portability without floating-point units.
 */

typedef int32_t fixed;

#define FIXED_SHIFT        16
#define FIXED_ONE          (1 << FIXED_SHIFT)         /* 65536 */
#define FIXED_HALF         (1 << (FIXED_SHIFT - 1))   /* 32768 */
#define FIXED_PI           205887                     /* 3.14159265 * 65536 */
#define FIXED_TWO_PI       411775                     /* 6.28318530 * 65536 */

#define TO_FIXED(x)        ((fixed)((int32_t)(x) << FIXED_SHIFT))
#define FROM_FIXED(x)      ((int32_t)((x) >> FIXED_SHIFT))
#define FROM_FIXED_ROUND(x) ((int32_t)(((x) + FIXED_HALF) >> FIXED_SHIFT))

static inline fixed fixed_mul(fixed a, fixed b) {
    return (fixed)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

static inline fixed fixed_div(fixed a, fixed b) {
    if (b == 0) return (a >= 0) ? 0x7FFFFFFF : -0x7FFFFFFF;
    return (fixed)((((int64_t)a) << FIXED_SHIFT) / (int64_t)b);
}

#define FMUL(a, b) fixed_mul(a, b)
#define FDIV(a, b) fixed_div(a, b)
#define FABS(a)    ((a) < 0 ? -(a) : (a))
#define FMIN(a, b) ((a) < (b) ? (a) : (b))
#define FMAX(a, b) ((a) > (b) ? (a) : (b))

static inline fixed fixed_clamp(fixed v, fixed min_v, fixed max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

/* Fast integer square root for 16.16 fixed point */
static inline fixed fixed_sqrt(fixed x) {
    if (x <= 0) return 0;
    uint64_t val = (uint64_t)x << FIXED_SHIFT;
    uint64_t res = 0;
    uint64_t bit = (uint64_t)1 << 62;

    while (bit > val) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (val >= res + bit) {
            val -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (fixed)res;
}

/* 256-angle sine lookup table (0 = 0 deg, 64 = 90 deg, 128 = 180 deg, 192 = 270 deg) */
extern const int32_t g_sin_lut[256];

static inline fixed fixed_sin(uint8_t angle) {
    return g_sin_lut[angle];
}

static inline fixed fixed_cos(uint8_t angle) {
    return g_sin_lut[(uint8_t)(angle + 64)];
}

/* Fast binary-radian atan2 returning 0..255 */
uint8_t fixed_atan2(fixed dy, fixed dx);

#endif /* FIXED_MATH_H */

