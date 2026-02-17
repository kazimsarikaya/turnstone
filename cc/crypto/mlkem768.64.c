/**
 * @file mlkem768.64.c
 * @brief ML-KEM-768 implementation (64-bit optimized)
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/mlkem768.h>
#include <utils.h>
#include <crypto/keccak.h>
#include <memory.h>
#include <random.h>
#include <logging.h>

MODULE("turnstone.lib.crypto");

#define MLKEM768_N 256
#define MLKEM768_N_INV_NORMAL 3303U
#define MLKEM768_N_INV_MONTGOMERY 512U
#define MLKEM768_Q 3329U
#define MLKEM768_Q_HALF (MLKEM768_Q / 2) // 1664
#define MLKEM768_Q_AND_HALF (MLKEM768_Q + MLKEM768_Q_HALF) // 4993
#define MLKEM768_Q_INV 62209U
#define MLKEM768_K 3
#define MLKEM768_ETA 2
#define MLKEM768_ERR_SAMPLE_BYTES (2 * MLKEM768_ETA * MLKEM768_N / 8) // 128 bytes for error sampling
#define MLKEM768_BITS 12
#define MLKEM768_U_BITS 10
#define MLKEM768_V_BITS 4
#define MLKEM768_Z_BYTES 32
#define MLKEM768_SECRET_MESSAGE_BYTES 32
#define MLKEM768_KR_K_BYTES 32
#define MLKEM768_SEED_LEN 32
#define MLKEM768_POLYBYTES (MLKEM768_N * MLKEM768_BITS / 8) // 256 * 1.5 = 384 bytes per polynomial
#define MLKEM768_U_PACKED_BYTES ((MLKEM768_K * MLKEM768_N * MLKEM768_U_BITS + 7) / 8) // 960 bytes for u
#define MLKEM768_V_PACKED_BYTES ((MLKEM768_N * MLKEM768_V_BITS + 7) / 8) // 128 bytes for v

#define MLKEM768_BARRETT_V 20159

typedef struct mlkem768_poly_t {
    uint16_t coeffs[MLKEM768_N];
} mlkem768_poly_t __attribute__((aligned(16)));

typedef struct mlkem768_polyvec_t {
    mlkem768_poly_t vec[MLKEM768_K];
} mlkem768_polyvec_t __attribute__((aligned(16)));

#define MLKEM768_USE_MONTGOMERY 1

#if MLKEM768_USE_MONTGOMERY
#define MLKEM768_INTT_MULTIPLIER MLKEM768_N_INV_MONTGOMERY
#else
#define MLKEM768_INTT_MULTIPLIER MLKEM768_N_INV_NORMAL
#endif

static const uint16_t mlkem768_zetas[] = {
#if MLKEM768_USE_MONTGOMERY
    2285, 2571, 2970, 1812, 1493, 1422,  287,  202,
    3158,  622, 1577,  182,  962, 2127, 1855, 1468,
    573, 2004,  264,  383, 2500, 1458, 1727, 3199,
    2648, 1017,  732,  608, 1787,  411, 3124, 1758,
    1223,  652, 2777, 1015, 2036, 1491, 3047, 1785,
    516, 3321, 3009, 2663, 1711, 2167,  126, 1469,
    2476, 3239, 3058,  830,  107, 1908, 3082, 2378,
    2931,  961, 1821, 2604,  448, 2264,  677, 2054,
    2226,  430,  555,  843, 2078,  871, 1550,  105,
    422,  587,  177, 3094, 3038, 2869, 1574, 1653,
    3083,  778, 1159, 3182, 2552, 1483, 2727, 1119,
    1739,  644, 2457,  349,  418,  329, 3173, 3254,
    817, 1097,  603,  610, 1322, 2044, 1864,  384,
    2114, 3193, 1218, 1994, 2455,  220, 2142, 1670,
    2144, 1799, 2051,  794, 1819, 2475, 2459,  478,
    3221, 3021,  996,  991,  958, 1869, 1522, 1628,
#else
    1, 1729, 2580, 3289, 2642, 630, 1897, 848,
    1062, 1919, 193, 797, 2786, 3260, 569, 1746,
    296, 2447, 1339, 1476, 3046, 56, 2240, 1333,
    1426, 2094, 535, 2882, 2393, 2879, 1974, 821,
    289, 331, 3253, 1756, 1197, 2304, 2277, 2055,
    650, 1977, 2513, 632, 2865, 33, 1320, 1915,
    2319, 1435, 807, 452, 1438, 2868, 1534, 2402,
    2647, 2617, 1481, 648, 2474, 3110, 1227, 910,
    17, 2761, 583, 2649, 1637, 723, 2288, 1100,
    1409, 2662, 3281, 233, 756, 2156, 3015, 3050,
    1703, 1651, 2789, 1789, 1847, 952, 1461, 2687,
    939, 2308, 2437, 2388, 733, 2337, 268, 641,
    1584, 2298, 2037, 3220, 375, 2549, 2090, 1645,
    1063, 319, 2773, 757, 2099, 561, 2466, 2594,
    2804, 1092, 403, 1026, 1143, 2150, 2775, 886,
    1722, 1212, 1874, 1029, 2110, 2935, 885, 2154,
#endif
};

_Static_assert(ARRAY_SIZE(mlkem768_zetas) == (MLKEM768_N / 2), "Zetas array size mismatch");

static const uint16_t mlkem768_basemul_zetas[] = {
#if MLKEM768_USE_MONTGOMERY
    2226, 1103,  430, 2899,  555, 2774,  843, 2486,
    2078, 1251,  871, 2458, 1550, 1779,  105, 3224,
    422, 2907,  587, 2742,  177, 3152, 3094,  235,
    3038,  291, 2869,  460, 1574, 1755, 1653, 1676,
    3083,  246,  778, 2551, 1159, 2170, 3182,  147,
    2552,  777, 1483, 1846, 2727,  602, 1119, 2210,
    1739, 1590,  644, 2685, 2457,  872,  349, 2980,
    418, 2911,  329, 3000, 3173,  156, 3254,   75,
    817, 2512, 1097, 2232,  603, 2726,  610, 2719,
    1322, 2007, 2044, 1285, 1864, 1465,  384, 2945,
    2114, 1215, 3193,  136, 1218, 2111, 1994, 1335,
    2455,  874,  220, 3109, 2142, 1187, 1670, 1659,
    2144, 1185, 1799, 1530, 2051, 1278,  794, 2535,
    1819, 1510, 2475,  854, 2459,  870,  478, 2851,
    3221,  108, 3021,  308,  996, 2333,  991, 2338,
    958, 2371, 1869, 1460, 1522, 1807, 1628, 1701,
#else
    17,     3312,     2761,     568,     583,     2746,     2649,     680,
    1637,     1692,     723,     2606,     2288,     1041,     1100,     2229,
    1409,     1920,     2662,     667,     3281,     48,     233,     3096,
    756,     2573,     2156,     1173,     3015,     314,     3050,     279,
    1703,     1626,     1651,     1678,     2789,     540,     1789,     1540,
    1847,     1482,     952,     2377,     1461,     1868,     2687,     642,
    939,     2390,     2308,     1021,     2437,     892,     2388,     941,
    733,     2596,     2337,     992,     268,     3061,     641,     2688,
    1584,     1745,     2298,     1031,     2037,     1292,     3220,     109,
    375,     2954,     2549,     780,     2090,     1239,     1645,     1684,
    1063,     2266,     319,     3010,     2773,     556,     757,     2572,
    2099,     1230,     561,     2768,     2466,     863,     2594,     735,
    2804,     525,     1092,     2237,     403,     2926,     1026,     2303,
    1143,     2186,     2150,     1179,     2775,     554,     886,     2443,
    1722,     1607,     1212,     2117,     1874,     1455,     1029,     2300,
    2110,     1219,     2935,     394,     885,     2444,     2154,     1175,
#endif
};

_Static_assert(ARRAY_SIZE(mlkem768_basemul_zetas) == (MLKEM768_N / 2), "Basemul zetas array size mismatch");


static inline uint16_t mlkem768_reduce_once(uint16_t a) {
    // Step 1: subtract Q
    int16_t r = (int16_t)(a - MLKEM768_Q);

    // Step 2: if negative, add Q back (branchless)
    r += (r >> 15) & MLKEM768_Q;

    return (uint16_t)r;
}

static inline uint16_t mlkem768_barrett_reduce(uint32_t a) {
    // 1. Use uint64_t for the wide multiplication
    // 20159 is (2^26 / 3329), so we multiply and shift right by 26
    uint32_t t = (uint32_t)(((uint64_t)a * MLKEM768_BARRETT_V) >> 26);

    // 2. Calculate the remainder
    uint32_t r = a - t * MLKEM768_Q;

    // 3. If r is negative, add Q back (branchless)
    r += (r >> 15) & MLKEM768_Q;

    // 3. Final reduction to ensure r < Q
    return mlkem768_reduce_once((uint16_t)r);
}

static inline uint16_t mlkem768_montgomery_reduce(uint32_t a) {
#if MLKEM768_USE_MONTGOMERY
    // 1. Calculate t = a * Q_INV mod R (65536)
    // We only need the lower 16 bits, so uint16_t is perfect here.
    uint16_t t = (uint16_t)((uint32_t)a * (uint32_t)MLKEM768_Q_INV);

    // 2. Calculate r = (a - t * Q) / R
    // We use int32_t because (t * Q) can be larger than 'a', making the result negative.
    // This is mathematically equivalent to (a + (R - t)*Q) / R in some implementations.
    int32_t r = ((int32_t)a - (int32_t)t * (int32_t)MLKEM768_Q) >> 16;

    // 3. Conditional add Q if the result is negative to bring it to [0, Q)
    if (r < 0) {
        r += MLKEM768_Q;
    }

    return (uint16_t)r;
#else
    return mlkem768_barrett_reduce((int32_t)a);
#endif
}

static inline uint16_t mlkem768_field_add(uint16_t a, uint16_t b) {
    return mlkem768_reduce_once(a + b);
}

static inline uint16_t mlkem768_field_sub(uint16_t a, uint16_t b) {
    return mlkem768_reduce_once(a + MLKEM768_Q - b);
}

static inline uint16_t mlkem768_field_mul(uint16_t a, uint16_t b) {
    return mlkem768_montgomery_reduce((uint32_t)a * b);
}

static inline uint16_t mlkem768_field_sub_mul(uint16_t a, uint16_t b, uint16_t c) {
    uint16_t sub_part = mlkem768_field_sub(a, b); // (a - b) mod Q
    return mlkem768_field_mul(sub_part, c); // ((a - b) * c) mod Q
}

static inline uint16_t mlkem768_to_montgomery(uint16_t a) {
#if MLKEM768_USE_MONTGOMERY
    return mlkem768_montgomery_reduce((uint32_t)a * 1353ULL);
#else
    return mlkem768_montgomery_reduce((uint32_t)a * 1); // Just reduce to ensure it's in [0, Q)
#endif
}

static inline uint16_t mlkem768_sample_ntt_to_montgomery(uint16_t a) {
#if MLKEM768_USE_MONTGOMERY
    return mlkem768_montgomery_reduce((uint32_t)a * 1353ULL);
#else
    return a;
#endif
}

static inline uint16_t mlkem768_from_montgomery(uint16_t a) {
    return mlkem768_montgomery_reduce((uint32_t)a * 1);
}

static void mlkem768_sample_ntt(mlkem768_poly_t* poly, const uint8_t seed[MLKEM768_SEED_LEN], uint8_t i, uint8_t j) {
    shake128_ctx_t* ctx = shake128_init();

    shake128_update(ctx, seed, MLKEM768_SEED_LEN);
    shake128_update(ctx, &i, 1);
    shake128_update(ctx, &j, 1);

    size_t count = 0;

    while (count < MLKEM768_N) {
        uint8_t* buf = shake128_next(ctx, 3);

        uint16_t d1 = buf[0] | ((buf[1] & 0x0F) << 8);
        uint16_t d2 = (buf[1] >> 4) | (buf[2] << 4);

        memory_free(buf);

        if (d1 < MLKEM768_Q && count < MLKEM768_N) {
            poly->coeffs[count++] = mlkem768_sample_ntt_to_montgomery(d1); // Move d1 to Montgomery Domain
        }
        if (d2 < MLKEM768_Q && count < MLKEM768_N) {
            poly->coeffs[count++] = mlkem768_sample_ntt_to_montgomery(d2); // Move d2 to Montgomery Domain
        }
    }

    memory_free(ctx);
}

static inline void mlkem768_ntt_butterfly(uint16_t * a, uint16_t zeta, uint32_t j, uint32_t len) {
    uint16_t t = mlkem768_field_mul(zeta, a[j + len]);

    a[j + len] = mlkem768_field_sub(a[j], t);
    a[j] = mlkem768_field_add(a[j], t);
}

static inline void mlkem768_intt_butterfly(uint16_t * a, uint16_t zeta, uint32_t j, uint32_t len) {
    uint16_t t0 = a[j];
    uint16_t t1 = a[j + len];

    a[j] = mlkem768_field_add(t1, t0);
    a[j + len] = mlkem768_field_sub_mul(t1, t0, zeta); // (t0 - t1) * zeta mod Q
}


static void mlkem768_ntt(mlkem768_poly_t* poly) {
    uint32_t len, start, j, k;
    k = 1; // Index for the zetas table

    // Level loop: starts at distance 128, halves each time
    for (len = ARRAY_SIZE(mlkem768_zetas); len >= 2; len >>= 1) {
        for (start = 0; start < MLKEM768_N; start += 2 * len) {
            uint16_t zeta = mlkem768_zetas[k++];
            for (j = start; j < start + len; j++) {
                mlkem768_ntt_butterfly(poly->coeffs, zeta, j, len);
            }
        }
    }
}

static void mlkem768_intt(mlkem768_poly_t * poly) {
    size_t len, start, j;
    size_t k = ARRAY_SIZE(mlkem768_zetas) - 1; // last index of zetas

    for (len = 2; len <= ARRAY_SIZE(mlkem768_zetas); len <<= 1) { // must go up to N
        for (start = 0; start < MLKEM768_N; start += 2 * len) {
            uint32_t zeta = mlkem768_zetas[k--];
            for (j = start; j < start + len; j++) {
                mlkem768_intt_butterfly(poly->coeffs, zeta, j, len);
            }
        }
    }

    for (size_t i = 0; i < MLKEM768_N; i++) {
        poly->coeffs[i] = mlkem768_field_mul(poly->coeffs[i], MLKEM768_INTT_MULTIPLIER);
    }
}

// Multiples two degree-1 polynomials modulo (X^2 - zeta)
static void mlkem768_basemul(uint16_t r[2], const uint16_t a[2], const uint16_t b[2], uint16_t zeta) {
    uint16_t r0, r1, r0_right, r0_left, r1_right, r1_left;

    r0_right = mlkem768_field_mul(a[1], b[1]); // a1 * b1
    r0_right = mlkem768_field_mul(r0_right, zeta); // a1 * b1 * zeta
    r0_left  = mlkem768_field_mul(a[0], b[0]); // a0 * b0 + a1 * b1 * zeta
    r0 = mlkem768_field_add(r0_left, r0_right);

    r1_right = mlkem768_field_mul(a[0], b[1]); // a0 * b1
    r1_left  = mlkem768_field_mul(a[1], b[0]); // a1 * b0 + a0 * b1
    r1 = mlkem768_field_add(r1_left, r1_right);

    r[0] = (uint16_t)r0;
    r[1] = (uint16_t)r1;
}


static void mlkem768_poly_basemul_montgomery(mlkem768_poly_t *       r,
                                             const mlkem768_poly_t * a,
                                             const mlkem768_poly_t * b) {
    for (size_t i = 0; i < ARRAY_SIZE(mlkem768_basemul_zetas); i++) {
        // Grab the specific zeta for this pair directly
        uint16_t zeta = mlkem768_basemul_zetas[i];

        // Multiply degree-1 polynomial pair i
        // r[2i], r[2i+1] = (a[2i], a[2i+1]) * (b[2i], b[2i+1]) mod (x^2 - zeta)
        mlkem768_basemul(&r->coeffs[2 * i],
                         &a->coeffs[2 * i],
                         &b->coeffs[2 * i],
                         zeta);
    }
}

static void mlkem768_polyvec_basemul_acc_montgomery(mlkem768_poly_t *          r,
                                                    const mlkem768_polyvec_t * a,
                                                    const mlkem768_polyvec_t * b){
    mlkem768_poly_t t;

    // r = a[0] * b[0]
    mlkem768_poly_basemul_montgomery(r, &a->vec[0], &b->vec[0]);

    for (uint32_t i = 1; i < MLKEM768_K; i++) {
        mlkem768_poly_basemul_montgomery(&t, &a->vec[i], &b->vec[i]);

        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            r->coeffs[j] = mlkem768_field_add(r->coeffs[j], t.coeffs[j]);
        }
    }
}


static void mlkem768_polyvec_matrix_mul(mlkem768_polyvec_t * r, const mlkem768_polyvec_t a[MLKEM768_K], const mlkem768_polyvec_t * s) {
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        mlkem768_polyvec_basemul_acc_montgomery(&r->vec[i], &a[i], s);
    }
}

// Serializes a polynomial into 384 bytes (256 * 1.5)
static void mlkem768_poly_tobytes(uint8_t * r, const mlkem768_poly_t * a) {
    for (uint32_t i = 0; i < MLKEM768_N / 2; i++) {
        // Map to [0, Q-1] range before serializing
        uint16_t t0 = mlkem768_reduce_once(a->coeffs[2 * i]);
        uint16_t t1 = mlkem768_reduce_once(a->coeffs[2 * i + 1]);

        r[3 * i + 0] = (uint8_t)(t0 & 0xFF);
        r[3 * i + 1] = (uint8_t)((t0 >> 8) | ((t1 & 0x0F) << 4));
        r[3 * i + 2] = (uint8_t)(t1 >> 4);
    }
}

// Deserializes 384 bytes back into a polynomial
static void mlkem768_poly_frombytes(mlkem768_poly_t * r, const uint8_t * a) {
    for (uint32_t i = 0; i < MLKEM768_N / 2; i++) {
        r->coeffs[2 * i + 0] = (a[3 * i + 0] >> 0) | ((uint16_t)(a[3 * i + 1] & 0x0F) << 8);
        r->coeffs[2 * i + 1] = (a[3 * i + 1] >> 4) | ((uint16_t)a[3 * i + 2] << 4);
    }
}

#if 0
static uint16_t mlkem768_compress(uint16_t x, uint8_t d) {
    // We want to compute (x * 2ᵈ) / q, rounded to nearest integer, with 1/2
    // rounding up (see FIPS 203, Section 2.3).

    // Barrett reduction produces a quotient and a remainder in the range [0, 2q),
    // such that dividend = quotient * q + remainder.
    uint64_t dividend = x;
    dividend <<= d; // Multiply by 2^d to prepare for scaling
    uint64_t quotient  = ((dividend * 20159) + (1 << 25)) >> 26; // Using a fixed-point multiplier for 1/3329 with 26 bits of precision
    uint64_t remainder = dividend - quotient * MLKEM768_Q; // Compute the remainder

    // Since the remainder is in the range [0, 2q), not [0, q), we need to
    // portion it into three spans for rounding.
    //
    // [ 0,       q/2     ) -> round to 0
    // [ q/2,     q + q/2 ) -> round to 1
    // [ q + q/2, 2q      ) -> round to 2
    //
    // We can convert that to the following logic: add 1 if remainder > q/2,
    // then add 1 again if remainder > q + q/2.
    //
    // Note that if remainder > x, then ⌊x⌋ - remainder underflows, and the top
    // bit of the difference will be set.
    quotient += ((MLKEM768_Q_HALF - remainder) >> 63) & 1;
    quotient += ((MLKEM768_Q_AND_HALF - remainder) >> 63) & 1;

    // quotient might have overflowed at this point, so reduce it by masking.
    uint64_t mask = (1 << d) - 1;
    return (uint16_t)(quotient & mask);
}
#endif

static uint16_t mlkem768_compress(uint16_t x, uint8_t d) {
    uint32_t t = ((uint32_t)x << d) + (MLKEM768_Q_HALF);
    t /= MLKEM768_Q;
    return (uint16_t)(t & ((1u << d) - 1));
}


static uint16_t decompress(uint16_t y, uint8_t d) {
    // We want to compute (y * q) / 2ᵈ, rounded to nearest integer, with 1/2
    // rounding up (see FIPS 203, Section 2.3).

    uint64_t dividend = (uint64_t)(y) * MLKEM768_Q; // y * q
    uint64_t quotient = dividend >> d; // (y * q) / 2ᵈ

    // The d'th least-significant bit of the dividend (the most significant bit
    // of the remainder) is 1 for the top half of the values that divide to the
    // same quotient, which are the ones that round up.
    quotient += dividend >> (d - 1) & 1;

    // quotient is at most (2¹¹-1) * q / 2¹¹ + 1 = 3328, so it didn't overflow.
    return (uint16_t)quotient;
}

static inline uint16_t mlkem768_compress10(uint16_t x) {
    return mlkem768_compress(x, MLKEM768_U_BITS);
}

// Decompress 10 bits back to Q
static inline uint16_t mlkem768_decompress10(uint16_t y) {
    return decompress(y, MLKEM768_U_BITS);
}

static void mlkem768_pack_u(uint8_t * r, mlkem768_polyvec_t * u) {
    for(uint32_t k = 0; k < MLKEM768_K; k++) {
        for(uint32_t i = 0; i < MLKEM768_N / 4; i++) {
            uint16_t t[4];
            for(uint32_t j = 0; j < 4; j++) {t[j] = mlkem768_compress10(u->vec[k].coeffs[4 * i + j]);}

            // Pack 4 coeffs (40 bits) into 5 bytes
            r[5 * i + 0] = (uint8_t)( t[0]        & 0xFF);
            r[5 * i + 1] = (uint8_t)((t[0] >> 8)  | ((t[1] & 0x3F) << 2));
            r[5 * i + 2] = (uint8_t)((t[1] >> 6)  | ((t[2] & 0x0F) << 4));
            r[5 * i + 3] = (uint8_t)((t[2] >> 4)  | ((t[3] & 0x03) << 6));
            r[5 * i + 4] = (uint8_t)( t[3] >> 2);

        }
        r += 320; // Move to next poly slot (256 * 10 / 8 = 320)
    }
}

static void mlkem768_unpack_u(mlkem768_polyvec_t * u, const uint8_t * ct) {
    for (uint32_t k = 0; k < MLKEM768_K; k++) {
        for (uint32_t i = 0; i < MLKEM768_N / 4; i++) {
            uint16_t t[4];
            const uint8_t * ptr = &ct[k * 320 + i * 5];

            // Extract 4 coefficients from 5 bytes
            t[0] = (uint16_t)(ptr[0]) | ((uint16_t)(ptr[1] & 0x03) << 8);
            t[1] = (uint16_t)(ptr[1] >> 2) | ((uint16_t)(ptr[2] & 0x0F) << 6);
            t[2] = (uint16_t)(ptr[2] >> 4) | ((uint16_t)(ptr[3] & 0x3F) << 4);
            t[3] = (uint16_t)(ptr[3] >> 6) | ((uint16_t)(ptr[4]) << 2);

            // Decompress and store
            for (uint32_t j = 0; j < 4; j++) {
                u->vec[k].coeffs[4 * i + j] = mlkem768_decompress10(t[j] & 0x3FF);
            }
        }
    }
}

static inline uint8_t mlkem768_compress4(uint16_t x) {
    return mlkem768_compress(x, 4);
}

static inline uint16_t mlkem768_decompress4(uint8_t y) {
    return decompress(y, 4);
}

static void mlkem768_pack_v(uint8_t * r, mlkem768_poly_t * v) {
    for(uint32_t i = 0; i < MLKEM768_N / 2; i++) {
        uint8_t t0 = mlkem768_compress4(v->coeffs[2 * i]);
        uint8_t t1 = mlkem768_compress4(v->coeffs[2 * i + 1]);
        r[i] = t0 | (t1 << 4); // Pack 2 coeffs into 1 byte
    }
}

static void mlkem768_unpack_v(mlkem768_poly_t * v, const uint8_t * ct) {
    for (uint32_t i = 0; i < MLKEM768_N / 2; i++) {
        // Extract 2 coefficients from 1 byte
        uint8_t t0 = ct[i] & 0x0F;
        uint8_t t1 = ct[i] >> 4;

        // Decompress and store
        v->coeffs[2 * i + 0] = mlkem768_decompress4(t0);
        v->coeffs[2 * i + 1] = mlkem768_decompress4(t1);
    }
}

static void mlkem768_cbd2(mlkem768_poly_t * r, const uint8_t* buf) {
    uint32_t t, d;
    uint16_t a, b;

    uint32_t* buf32 = (uint32_t*)buf; // Treat as 32-bit for easy processing

    for (uint32_t i = 0; i < MLKEM768_N / 8; i++) {
        // Pull 4 bytes to process 8 coefficients
        t  = buf32[i]; // Treat as 32-bit for easy bit-shifting
        d  = t & 0x55555555; // Mask odd bits
        d += (t >> 1) & 0x55555555; // Sum pairs of bits

        for (uint32_t j = 0; j < 8; j++) {
            a = (d >> (4 * j)) & 0x3;
            b = (d >> (4 * j + 2)) & 0x3;
            int16_t res = a - b;
            // Ensure it is in the range [0, Q) before moving on
            r->coeffs[8 * i + j] = res + ((res >> 15) & MLKEM768_Q);
        }
    }
}

static void mlkem768_sample_error_vec(mlkem768_polyvec_t * v, const uint8_t seed[MLKEM768_SEED_LEN], uint8_t nonce) {
    uint8_t extseed[MLKEM768_SEED_LEN + 1]; // 32 bytes for sigma + 1 byte for nonce

    memory_memcopy(seed, extseed, MLKEM768_SEED_LEN); // Copy sigma into extseed

    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        extseed[MLKEM768_SEED_LEN] = nonce++;

        // ML-KEM PRF: SHAKE256(sigma || counter)
        uint8_t* hash = shake256_hash(extseed, MLKEM768_SEED_LEN + 1, MLKEM768_ERR_SAMPLE_BYTES);

        mlkem768_cbd2(&v->vec[i], hash);

        memory_free(hash);
    }
}

static void mlkem768_sample_error_poly(mlkem768_poly_t * poly, const uint8_t seed[MLKEM768_SEED_LEN], uint8_t nonce) {
    uint8_t extseed[MLKEM768_SEED_LEN + 1]; // 32 bytes for sigma + 1 byte for nonce

    // 1. Prepare extended seed (sigma || nonce)
    memory_memcopy(seed, extseed, MLKEM768_SEED_LEN); // Copy sigma into extseed
    extseed[MLKEM768_SEED_LEN] = nonce;

    // 2. PRF: Squeeze noise from SHAKE256
    uint8_t* hash = shake256_hash(extseed, MLKEM768_SEED_LEN + 1, MLKEM768_ERR_SAMPLE_BYTES);

    // 3. Apply the CBD2 sampling you built earlier
    mlkem768_cbd2(poly, hash);
    memory_free(hash);
}

void mlkem768_keygen(uint8_t * pk, uint8_t * sk) {
    uint8_t seed[MLKEM768_SEED_LEN + 1];
    uint8_t public_seed[MLKEM768_SEED_LEN];
    uint8_t noise_seed[MLKEM768_SEED_LEN];
    mlkem768_polyvec_t a[MLKEM768_K], s, s_copy, e, pk_vec;

    // 1. Expand seed → rho + sigma
    get_random_bytes(seed, MLKEM768_SEED_LEN);
    seed[MLKEM768_SEED_LEN] = MLKEM768_K; // from spec we put k in the last byte of seed for keygen
    uint8_t* hash = sha3_512_hash(seed, MLKEM768_SEED_LEN + 1);
    memory_memcopy(hash, public_seed, MLKEM768_SEED_LEN); // rho
    memory_memcopy(hash + MLKEM768_SEED_LEN, noise_seed, MLKEM768_SEED_LEN); // sigma
    memory_free(hash);

    // 2. Sample matrix A
    for (uint8_t i = 0; i < MLKEM768_K; i++) {
        for (uint8_t j = 0; j < MLKEM768_K; j++) {
            mlkem768_sample_ntt(&a[i].vec[j], public_seed, j, i);
        }
    }

    // 3. Sample s and e (normal domain)
    mlkem768_sample_error_vec(&s, noise_seed, 0);
    mlkem768_sample_error_vec(&e, noise_seed, MLKEM768_K); // Use a different nonce for e to ensure it's different from s

    // 4. NTT s and e
    for(uint32_t i = 0; i < MLKEM768_K; i++) {
        for(uint32_t j = 0; j < MLKEM768_N; j++) {
            // Move to Montgomery domain (factor of R)
            s.vec[i].coeffs[j] = mlkem768_to_montgomery(s.vec[i].coeffs[j]);
        }
        mlkem768_ntt(&s.vec[i]);
        for(uint32_t j = 0; j < MLKEM768_N; j++) {
            s_copy.vec[i].coeffs[j] = mlkem768_from_montgomery(s.vec[i].coeffs[j]);
        }
    }
    for(uint32_t i = 0; i < MLKEM768_K; i++) {
        for(uint32_t j = 0; j < MLKEM768_N; j++) {
            // Move to Montgomery domain (factor of R)
            e.vec[i].coeffs[j] = mlkem768_to_montgomery(e.vec[i].coeffs[j]);
        }
        mlkem768_ntt(&e.vec[i]);
    }

    // 5. t_hat = A_hat·s_hat + e_hat  (all Montgomery + NTT)
    mlkem768_polyvec_matrix_mul(&pk_vec, a, &s);

    memory_memclean(a, sizeof(a)); // Clean up the temporary a which is not stored in either key
    memory_memclean(s.vec, sizeof(s.vec)); // Clean up the temporary s which is stored in the secret key but we will store a non-Montgomery version

    for(uint32_t i = 0; i < MLKEM768_K; i++) {
        for(uint32_t j = 0; j < MLKEM768_N; j++) {
            pk_vec.vec[i].coeffs[j] = mlkem768_field_add(pk_vec.vec[i].coeffs[j], e.vec[i].coeffs[j]);

            // move from Montgomery domain back to normal ntt domain for storage in public key
            pk_vec.vec[i].coeffs[j] = mlkem768_from_montgomery(pk_vec.vec[i].coeffs[j]);
        }
    }

    // 6. Write public key: t_hat || rho
    for(uint32_t i = 0; i < MLKEM768_K; i++) {
        mlkem768_poly_tobytes(pk + i * MLKEM768_POLYBYTES, &pk_vec.vec[i]);
    }
    memory_memcopy(public_seed, pk + MLKEM768_PUBLICKEYBYTES - MLKEM768_SEED_LEN, MLKEM768_SEED_LEN);

    // 9. Secret key: s_hat | pk | H(pk) | z
    size_t offset = 0;
    for(uint32_t i = 0; i < MLKEM768_K; i++) {
        mlkem768_poly_tobytes(sk + offset, &s_copy.vec[i]);
        offset += MLKEM768_POLYBYTES;
    }

    memory_memclean(s_copy.vec, sizeof(s_copy.vec)); // Clean up the temporary s_copy which is stored in the secret key

    memory_memcopy(pk, sk + offset, MLKEM768_PUBLICKEYBYTES);
    offset += MLKEM768_PUBLICKEYBYTES;

    uint8_t* h_pk = sha3_256_hash(pk, MLKEM768_PUBLICKEYBYTES);
    memory_memcopy(h_pk, sk + offset, SHA3_256_OUTPUT_SIZE);
    memory_free(h_pk);
    offset += SHA3_256_OUTPUT_SIZE;

    get_random_bytes(sk + offset, MLKEM768_Z_BYTES);
}

static void mlkem768_encaps_internal(uint8_t * ct, const uint8_t * pk, const uint8_t * m, const uint8_t * random) {
    mlkem768_polyvec_t a_t[MLKEM768_K], r, e1, u, pk_vec;
    mlkem768_poly_t e2, v;

    // 1. Deserialize public key t (already in ntt domain)
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        mlkem768_poly_frombytes(&pk_vec.vec[i], pk + i * MLKEM768_POLYBYTES);

        // MOVE PK TO MONTGOMERY DOMAIN!
        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            pk_vec.vec[i].coeffs[j] = mlkem768_to_montgomery((uint32_t)pk_vec.vec[i].coeffs[j]);
        }
        // No NTT needed here because pk is already stored in NTT form
    }

    // 2. Generate matrix A^T in NTT domain using rho = pk[1184..1215]
    uint8_t rho[MLKEM768_SEED_LEN];
    memory_memcopy(pk + MLKEM768_PUBLICKEYBYTES - MLKEM768_SEED_LEN, rho, MLKEM768_SEED_LEN);

    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        for (uint32_t j = 0; j < MLKEM768_K; j++) {
            mlkem768_sample_ntt(&a_t[i].vec[j], rho, i, j); // note: keygen uses (j, i) for sampling A, so we flip to (i, j) here to get A^T
        }
    }

    // 3. Sample randomness and noise (all start in normal domain)
    mlkem768_sample_error_vec(&r, random, 0); // r   ← nonces 0,1,2
    mlkem768_sample_error_vec(&e1, random, 3); // e1  ← nonces 3,4,5
    mlkem768_sample_error_poly(&e2, random, 6); // e2  ← nonce 6

    // 4. r → Montgomery + NTT  (so it matches pk_vec domain for multiplication)
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            r.vec[i].coeffs[j] = mlkem768_to_montgomery((uint32_t)r.vec[i].coeffs[j]);
        }
        mlkem768_ntt(&r.vec[i]);
    }

    // 5. u = A^T · r    (in NTT+montgomery domain multiplication)
    mlkem768_polyvec_matrix_mul(&u, a_t, &r);

    // Bring u back to normal domain + add e1
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        // intt (A^T · r) back to normal domain (still in Montgomery form)
        mlkem768_intt(&u.vec[i]);
        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            // Move from Montgomery domain back to normal domain for addition with e1
            u.vec[i].coeffs[j] = mlkem768_from_montgomery(u.vec[i].coeffs[j]);
            // Add e1 (still in normal domain) and reduce to [0, Q)
            u.vec[i].coeffs[j] = mlkem768_field_add(u.vec[i].coeffs[j], e1.vec[i].coeffs[j]);
        }
    }

    // 6. v = t_hat · r_hat + e2 + encode(m)
    // t̂ and r̂ are both in NTT+Montgomery → result is correct in NTT+Montgomery
    mlkem768_polyvec_basemul_acc_montgomery(&v, &pk_vec, &r);

    // Bring v back to montgomery domain
    mlkem768_intt(&v);

    // Add e2 (still in normal domain)
    for (uint32_t i = 0; i < MLKEM768_N; i++) {
        v.coeffs[i] = mlkem768_from_montgomery(v.coeffs[i]);
        v.coeffs[i] = mlkem768_field_add(v.coeffs[i], e2.coeffs[i]);
    }

    // Encode message m into the high bits of v
    size_t coeff_idx = 0;
    for (size_t i = 0; i < MLKEM768_SECRET_MESSAGE_BYTES; i++) {
        for (int b = 0; b < 8; b++) {
            uint16_t bit = (m[i] >> b) & 1;

            // Add the message bit scaled to Q/2
            uint32_t val = (uint32_t)v.coeffs[coeff_idx] + (bit * 1665); // Use 1665 for (Q+1)/2

            // REDUCE IMMEDIATELY
            v.coeffs[coeff_idx++] = mlkem768_reduce_once(val);
        }
    }

    // 7. Compress & pack ciphertext
    mlkem768_pack_u(ct, &u);
    mlkem768_pack_v(ct + MLKEM768_U_PACKED_BYTES, &v);
}

void mlkem768_encaps(uint8_t * ct, uint8_t * ss, const uint8_t * pk) {
    uint8_t m[MLKEM768_SECRET_MESSAGE_BYTES];

    // --- 1. Get entropy for the message ---
    get_random_bytes(m, MLKEM768_SECRET_MESSAGE_BYTES); // 32 bytes from HW RNG

    // --- 2. Hash the public key: H(pk) ---
    uint8_t* h_pk = sha3_256_hash(pk, MLKEM768_PUBLICKEYBYTES);

    // --- 3. FO Transform: derive K and coins r ---
    sha3_512_ctx_t* g_ctx = sha3_512_init();
    sha3_512_update(g_ctx, m, MLKEM768_SECRET_MESSAGE_BYTES); // message m
    sha3_512_update(g_ctx, h_pk, SHA3_256_OUTPUT_SIZE); // H(pk)
    uint8_t* kr = sha3_512_final(g_ctx); // kr[0..31]=K, kr[32..63]=r

    memory_free(h_pk); // free H(pk)

    // --- 4. Encapsulate using internal function ---
    mlkem768_encaps_internal(ct, pk, m, kr + MLKEM768_SECRET_MESSAGE_BYTES); // pass r as randomness

    // --- 5. Set shared secret as K ---
    memory_memcopy(kr, ss, MLKEM768_SECRET_MESSAGE_BYTES);

    // --- 6. Clean up ---
    memory_free(kr);
    memory_memclean(m, MLKEM768_SECRET_MESSAGE_BYTES); // clear sensitive message from stack
}

static void mlkem768_decaps_internal(uint8_t * m_prime, const uint8_t * ct, const uint8_t * sk) {
    mlkem768_polyvec_t u, s;
    mlkem768_poly_t v, mp;

    // 1. Unpack ciphertext: u (compressed 10-bit) and v (compressed 4-bit)
    mlkem768_unpack_u(&u, ct);
    mlkem768_unpack_v(&v, ct + MLKEM768_U_PACKED_BYTES);

    // 2. Load secret s_hat
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        mlkem768_poly_frombytes(&s.vec[i], sk + i * MLKEM768_POLYBYTES);
    }

    // 3. Bring u to NTT domain (u was unpacked to normal domain)
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            // Move to Montgomery domain (factor of R)
            u.vec[i].coeffs[j] = mlkem768_to_montgomery((uint32_t)u.vec[i].coeffs[j]);
        }

        mlkem768_ntt(&u.vec[i]);
    }

    // s for Montgomery reduce
    for (uint32_t i = 0; i < MLKEM768_K; i++) {
        for (uint32_t j = 0; j < MLKEM768_N; j++) {
            s.vec[i].coeffs[j] = mlkem768_to_montgomery((uint32_t)s.vec[i].coeffs[j]);
        }
    }

    // 4. Compute ŵ = ŝ · û   (pointwise in NTT domain)
    // → result ŵ is in NTT domain (Montgomery scaling depends on impl)
    mlkem768_polyvec_basemul_acc_montgomery(&mp, &s, &u);

    // 5. Bring back to normal domain: w = INTT(ŵ)
    mlkem768_intt(&mp);

    // 6. Compute message polynomial candidate: m' = v - w   (v normal w in montgomery domain, so convert w out of montgomery first)
    for (uint32_t i = 0; i < MLKEM768_N; i++) {
        mp.coeffs[i] = mlkem768_from_montgomery(mp.coeffs[i]); // ensure in normal domain for message decoding
        mp.coeffs[i] = mlkem768_field_sub(v.coeffs[i], mp.coeffs[i]); // reduce to [0, Q)
    }

    // 7. Recover message bits from high bits of mp (simple thresholding)
    for (int i = 0; i < MLKEM768_SECRET_MESSAGE_BYTES; i++) {
        m_prime[i] = 0;
        for (int j = 0; j < 8; j++) {
            int idx = 8 * i + j;

            uint32_t t = mp.coeffs[idx];

            // Decide bit: 1 if closer to Q/2 than to 0 (or Q)
            // In ML-KEM this is effectively: if t >= Q/2 then 1 else 0
            uint8_t bit = mlkem768_compress(t, 1) & 1;

            // MSB first
            m_prime[i] |= (bit << j);
        }
    }
}

void mlkem768_decaps(uint8_t * ss, const uint8_t * ct, const uint8_t * sk) {
    uint8_t m_prime[MLKEM768_SECRET_MESSAGE_BYTES];
    uint8_t ct_prime[MLKEM768_CIPHERTEXTBYTES];
    uint8_t h_pk[SHA3_256_OUTPUT_SIZE];
    uint8_t z[MLKEM768_SEED_LEN];

    // 1. Recover m' from ciphertext
    mlkem768_decaps_internal(m_prime, ct, sk);

    // 2. Extract H(pk) and z from secret key
    memory_memcopy(sk + MLKEM768_POLYBYTES * MLKEM768_K + MLKEM768_PUBLICKEYBYTES, h_pk, SHA3_256_OUTPUT_SIZE);
    memory_memcopy(sk + MLKEM768_SECRETKEYBYTES - MLKEM768_SEED_LEN, z, MLKEM768_SEED_LEN);

    // 3. Re-derive (K', r') = G(m' || H(pk))
    sha3_512_ctx_t* g_ctx = sha3_512_init();
    sha3_512_update(g_ctx, m_prime, MLKEM768_SECRET_MESSAGE_BYTES);
    sha3_512_update(g_ctx, h_pk, SHA3_256_OUTPUT_SIZE);
    uint8_t* kr_prime = sha3_512_final(g_ctx);
    memory_memclean(h_pk, SHA3_256_OUTPUT_SIZE);

    // 4. Re-encrypt using pk from sk and r' to get expected ciphertext
    mlkem768_encaps_internal(ct_prime, sk + MLKEM768_POLYBYTES * MLKEM768_K, m_prime, kr_prime + MLKEM768_SECRET_MESSAGE_BYTES);

    // 5. Compare ciphertexts
    uint32_t mismatch = 0;
    for (uint32_t i = 0; i < MLKEM768_CIPHERTEXTBYTES; i++) {
        mismatch |= (ct[i] ^ ct_prime[i]);
    }

    // 6. Derive shared secret
    if (mismatch == 0) {
        memory_memcopy(kr_prime, ss, MLKEM768_SHARED_SECRET_BYTES); // K' = kr_prime[0..31]
        memory_free(kr_prime);
    } else {
        memory_free(kr_prime);
        shake256_ctx_t* kdf_ctx = shake256_init();
        shake256_update(kdf_ctx, z, MLKEM768_Z_BYTES); // z from secret key
        shake256_update(kdf_ctx, ct, MLKEM768_CIPHERTEXTBYTES); // H(ct) is effectively just ct for the KDF input since it is already hashed in the encaps step
        uint8_t* ss_full = shake256_final(kdf_ctx, MLKEM768_SHARED_SECRET_BYTES);
        memory_memcopy(ss_full, ss, MLKEM768_SHARED_SECRET_BYTES);
        memory_free(ss_full);
    }
}
