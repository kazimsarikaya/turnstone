/**
 * @file math_f32.h
 * @brief Math library for 32-bit floating point operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___MATH_F32_H
#define ___MATH_F32_H

#include <types.h>

// Constants
float32_t math_pi_f32(void);
float32_t math_tau_f32(void);
float32_t math_epsilon_f32(void);

// Basic utilities
float32_t math_fabs_f32(float32_t x);
float32_t math_min_f32(float32_t a, float32_t b);
float32_t math_max_f32(float32_t a, float32_t b);
float32_t math_clamp_f32(float32_t x, float32_t min, float32_t max);
float32_t math_lerp_f32(float32_t a, float32_t b, float32_t t);
float32_t math_sign_f32(float32_t x);

// Trigonometric
float32_t math_sin_f32(float32_t x);
float32_t math_cos_f32(float32_t x);
float32_t math_tan_f32(float32_t x);
float32_t math_asin_f32(float32_t x);
float32_t math_acos_f32(float32_t x);
float32_t math_atan_f32(float32_t x);
float32_t math_atan2_f32(float32_t y, float32_t x);

// Power and roots
float32_t math_sqrt_f32(float32_t x);
float32_t math_rsqrt_f32(float32_t x);
float32_t math_pow_f32(float32_t base, float32_t exp);

// Rounding and modulo
float32_t math_floor_f32(float32_t x);
float32_t math_ceil_f32(float32_t x);
float32_t math_round_f32(float32_t x);
float32_t math_fmod_f32(float32_t x, float32_t y);

#endif // ___MATH_F32_H
