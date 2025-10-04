/**
 * @file math_f32.64.c
 * @brief Math library for 32-bit floating point operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <math_f32.h>

MODULE("turnstone.lib.math.f32");

// Helper union for bit manipulation (assumes IEEE 754 float)
typedef union {
    float32_t    f;
    unsigned int u;
} f32_u;

// Constants
float32_t math_pi_f32(void) {
    return 3.141592653589793f;
}
float32_t math_tau_f32(void) {
    return 6.283185307179586f;
}
float32_t math_epsilon_f32(void) {
    return 1e-6f;
}

// Basic utilities
float32_t math_fabs_f32(float32_t x) {
    f32_u u = {x};
    u.u &= 0x7FFFFFFFu;
    return u.f;
}

float32_t math_min_f32(float32_t a, float32_t b) {
    return (a < b) ? a : b;
}

float32_t math_max_f32(float32_t a, float32_t b) {
    return (a > b) ? a : b;
}

float32_t math_clamp_f32(float32_t x, float32_t min, float32_t max) {
    return math_max_f32(min, math_min_f32(x, max));
}

float32_t math_lerp_f32(float32_t a, float32_t b, float32_t t) {
    return a + t * (b - a);
}

float32_t math_sign_f32(float32_t x) {
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

// Precomputed factorial ratios for Taylor series recurrence
// term_n = term_(n-1) * (-x^2 / ratio)
static const float32_t math_sin_fact_ratio_f32[7] = {
    6.0f, // 3!/1!
    20.0f, // 5!/3!
    42.0f, // 7!/5!
    72.0f, // 9!/7!
    110.0f, // 11!/9!
    156.0f, // 13!/11!
    210.0f // 15!/13!
};

// Sin using Taylor series with recurrence and no division
float32_t math_sin_f32(float32_t x) {
    // Range reduction: x mod 2pi, then to [-pi, pi]
    float32_t tau = math_tau_f32();
    x = math_fmod_f32(x, tau);
    if (x > math_pi_f32()) x -= tau;
    if (x < -math_pi_f32()) x += tau;

    float32_t x2 = x * x;
    float32_t term = x; // first term x^1 / 1!
    float32_t result = term;

    for (int n = 0; n < 7; ++n) {
        term *= -x2 / math_sin_fact_ratio_f32[n]; // recurrence, division by precomputed ratio
        result += term;
    }

    return result;
}

// Precomputed factorial ratios for cosine Taylor series
// term_n = term_(n-1) * (-x^2 / ratio)
static const float32_t math_cos_fact_ratio_f32[7] = {
    2.0f, // 2!/0!
    12.0f, // 4!/2!
    30.0f, // 6!/4!
    56.0f, // 8!/6!
    90.0f, // 10!/8!
    132.0f, // 12!/10!
    182.0f // 14!/12!
};

// Cos using Taylor series with recurrence and no factorial/pow
float32_t math_cos_f32(float32_t x) {
    // Range reduction: x mod 2pi, then to [-pi, pi]
    float32_t tau = math_tau_f32();
    x = math_fmod_f32(x, tau);
    if (x > math_pi_f32()) x -= tau;
    if (x < -math_pi_f32()) x += tau;

    float32_t x2 = x * x;
    float32_t term = 1.0f; // first term x^0 / 0!
    float32_t result = term;

    for (int n = 0; n < 7; ++n) {
        term *= -x2 / math_cos_fact_ratio_f32[n]; // recurrence, division by precomputed ratio
        result += term;
    }

    return result;
}

// Tan as sin/cos (with check for cos ~0)
float32_t math_tan_f32(float32_t x) {
    float32_t cos_x = math_cos_f32(x);
    if (math_fabs_f32(cos_x) < math_epsilon_f32()) return 0.0f;  // Avoid division by zero, return 0 as approx
    return math_sin_f32(x) / cos_x;
}

// Asin using identity: asin(x) = atan(x / sqrt(1 - x^2))
float32_t math_asin_f32(float32_t x) {
    if (math_fabs_f32(x) > 1.0f) return 0.0f;  // Invalid, return 0
    return math_atan_f32(x / math_sqrt_f32(1.0f - x * x));
}

// Acos using identity: acos(x) = pi/2 - asin(x)
float32_t math_acos_f32(float32_t x) {
    return math_pi_f32() / 2.0f - math_asin_f32(x);
}

float32_t math_atan_f32(float32_t x) {
    if (math_fabs_f32(x) > 1.0f) {
        // Use identity: atan(x) = sign(x) * (pi/2 - atan(1/|x|))
        float32_t inv = 1.0f / math_fabs_f32(x);
        return math_sign_f32(x) * (math_pi_f32() / 2.0f - math_atan_f32(inv));
    }

    float32_t x2 = x * x;
    float32_t term = x; // first term x
    float32_t result = term;

    for (int n = 1; n < 8; ++n) {
        // recurrence: term_n = -term_(n-1) * x^2 * (2n-1)/(2n+1)
        term *= -x2 * (2.0f * n - 1.0f) / (2.0f * n + 1.0f);
        result += term;
    }

    return result;
}

// Atan2 handling quadrants
float32_t math_atan2_f32(float32_t y, float32_t x) {
    if (x == 0.0f) {
        if (y > 0.0f) return math_pi_f32() / 2.0f;
        if (y < 0.0f) return -math_pi_f32() / 2.0f;
        return 0.0f;
    }
    float32_t atan = math_atan_f32(y / x);
    if (x < 0.0f) {
        if (y >= 0.0f) atan += math_pi_f32();
        else atan -= math_pi_f32();
    }
    return atan;
}

// Sqrt using Newton's method (initial guess with bit hack)
float32_t math_sqrt_f32(float32_t x) {
    if (x <= 0.0f) return 0.0f;
    float32_t guess = x;
    for (int i = 0; i < 8; ++i) { // 8 iterations for good accuracy
        guess = 0.5f * (guess + x / guess);
    }
    return guess;
}

// Rsqrt using Quake III fast inverse sqrt (bit hack + 1 Newton iteration)
float32_t math_rsqrt_f32(float32_t x) {
    if (x <= 0.0f) return 0.0f;
    f32_u u = {x};
    u.u = 0x5F3759DFu - (u.u >> 1);
    float32_t y = u.f;
    y *= 1.5f - 0.5f * x * y * y; // 1 Newton iteration
    return y;
}

// Natural log using Taylor series around 1: ln(1 + z) = z - z^2/2 + z^3/3 - ...
static float32_t math_log_helper_f32(float32_t x) {
    if (x <= 0.0f) return 0.0f;  // Invalid

    f32_u u = {x};
    int exp = ((u.u >> 23) & 0xFF) - 127; // extract exponent
    u.u = (u.u & 0x007FFFFF) | 0x3F800000; // normalize mantissa to [1,2)
    float32_t z = u.f - 1.0f; // z in [0,1)

    float32_t term = z;
    float32_t result = term;

    for (int n = 2; n <= 10; ++n) {
        term *= -z * (float32_t)(n - 1) / (float32_t)n; // recurrence: term_n = -term_(n-1) * z * (n-1)/n
        result += term;
    }

    return result + (float32_t)exp * 0.6931471805599453f; // ln(2) * exp
}

// Exponential using Taylor recurrence
static float32_t math_exp_helper_f32(float32_t x) {
    float32_t term = x;
    float32_t result = 1.0f + term;

    for (int n = 2; n <= 10; ++n) {
        term *= x / (float32_t)n; // term_n = term_(n-1) * x / n
        result += term;
    }

    return result;
}

// Pow using log * exp
float32_t math_pow_f32(float32_t base, float32_t exp) {
    if (base <= 0.0f) return 0.0f;
    return math_exp_helper_f32(math_log_helper_f32(base) * exp);
}

// Floor using bit manipulation
float32_t math_floor_f32(float32_t x) {
    f32_u u = {x};
    int e = ((u.u >> 23) & 0xFF) - 127;
    if (e < 0) return (x < 0.0f) ? -1.0f : 0.0f;
    if (e >= 23) return x;
    unsigned int mask = 0x007FFFFF >> e;
    if ((u.u & mask) == 0) return x;  // Already integer
    u.u &= ~mask; // Truncate
    if (u.u & 0x80000000) u.f -= 1.0f;  // Negative, adjust down
    return u.f;
}

// Ceil as -floor(-x)
float32_t math_ceil_f32(float32_t x) {
    return -math_floor_f32(-x);
}

// Round to nearest (add 0.5 and floor)
float32_t math_round_f32(float32_t x) {
    return math_floor_f32(x + 0.5f);
}

// Fmod using floor
float32_t math_fmod_f32(float32_t x, float32_t y) {
    if (y == 0.0f) return 0.0f;  // Simple handling
    return x - y * math_floor_f32(x / y);
}
