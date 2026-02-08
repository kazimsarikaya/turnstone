/**
 * @file ellipticcurve.64.c
 * @brief Elliptic Curve Cryptography (ECC) Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/ellipticcurve.h>
#include <memory.h>
#include <bigint.h>
#include <random.h>
#include <crypto/sha2.h>
#include <crypto/pem.h>
#include <crypto/der.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.lib.crypto");

typedef struct ellipticcurve_private_key_t {
    bigint_t* d; // The 256-bit private scalar
} ellipticcurve_private_key_t;

typedef struct ellipticcurve_point_t {
    bigint_t* x; // The 256-bit X coordinate
    bigint_t* y; // The 256-bit Y coordinate
} ellipticcurve_point_t;

typedef struct ellipticcurve_jacobian_point_t {
    bigint_t* x; // The 256-bit X coordinate
    bigint_t* y; // The 256-bit Y coordinate
    bigint_t* z; // The 256-bit Z coordinate (for Jacobian representation)
} ellipticcurve_jacobian_point_t;

typedef struct ellipticcurve_curve_t {
    bigint_t*                       p; // prime modulus
    bigint_t*                       p_minus_2; // p - 2, used for modular inverse
    bigint_t*                       a; // curve coefficient a
    bigint_t*                       b; // curve coefficient b
    ellipticcurve_jacobian_point_t* g; // base point G
    bigint_t*                       n; // order of the base point G
    bigint_t*                       n_half; // n / 2, used for signature verification
} ellipticcurve_curve_t;


static ellipticcurve_jacobian_point_t* ellipticcurve_jacobian_point_create(void) {
    ellipticcurve_jacobian_point_t* point = memory_malloc(sizeof(ellipticcurve_jacobian_point_t));
    if (!point) {
        return NULL;
    }

    point->x = bigint_zero();
    point->y = bigint_zero();
    point->z = bigint_zero();

    if (!point->x || !point->y || !point->z) {
        if (point->x) {
            bigint_destroy(point->x);
        }
        if (point->y) {
            bigint_destroy(point->y);
        }
        if (point->z) {
            bigint_destroy(point->z);
        }
        memory_free(point);
        return NULL;
    }

    return point;
}

static void ellipticcurve_jacobian_point_destroy(ellipticcurve_jacobian_point_t* point) {
    if (!point) {
        return;
    }

    if (point->x) {
        bigint_destroy(point->x);
    }
    if (point->y) {
        bigint_destroy(point->y);
    }
    if (point->z) {
        bigint_destroy(point->z);
    }

    memory_free(point);
}

static ellipticcurve_point_t* ellipticcurve_point_create(void) {
    ellipticcurve_point_t* point = memory_malloc(sizeof(ellipticcurve_point_t));
    if (!point) {
        return NULL;
    }

    point->x = bigint_zero();
    point->y = bigint_zero();

    if (!point->x || !point->y) {
        if (point->x) {
            bigint_destroy(point->x);
        }
        if (point->y) {
            bigint_destroy(point->y);
        }
        memory_free(point);
        return NULL;
    }

    return point;
}

static void ellipticcurve_point_destroy(ellipticcurve_point_t* point) {
    if (!point) {
        return;
    }

    if (point->x) {
        bigint_destroy(point->x);
    }
    if (point->y) {
        bigint_destroy(point->y);
    }

    memory_free(point);
}

static ellipticcurve_jacobian_point_t* ellipticcurve_create_jacobian_point_at_infinity() {
    ellipticcurve_jacobian_point_t* point = ellipticcurve_jacobian_point_create();
    if (!point) {
        return NULL;
    }

    // Set Z to 0 to represent the point at infinity in Jacobian coordinates
    if (bigint_set_zero(point->z) != 0) {
        ellipticcurve_jacobian_point_destroy(point);
        return NULL;
    }

    return point;
}

static ellipticcurve_jacobian_point_t* ellipticcurve_jacobian_point_create_from_bytes(const uint8_t* x_bytes, const uint8_t* y_bytes) {
    if (!x_bytes || !y_bytes) {
        return NULL;
    }

    ellipticcurve_jacobian_point_t* point = ellipticcurve_jacobian_point_create();
    if (!point) {
        return NULL;
    }

    if (bigint_from_bytes(point->x, x_bytes, 32) != 0 ||
        bigint_from_bytes(point->y, y_bytes, 32) != 0 ||
        bigint_set_int64(point->z, 1) != 0) {
        ellipticcurve_jacobian_point_destroy(point);
        return NULL;
    }

    return point;
}

static boolean_t ellipticcurve_jacobian_point_is_at_infinity(const ellipticcurve_jacobian_point_t* point) {
    if (!point || !point->z) {
        return false;
    }

    // In Jacobian coordinates, the point at infinity is represented by Z = 0
    return bigint_is_zero(point->z);
}

static ellipticcurve_jacobian_point_t* ellipticcurve_secp256r1_create_g() {
    ellipticcurve_jacobian_point_t* g = ellipticcurve_jacobian_point_create();
    if (!g) {
        return NULL;
    }

    // Set G.x and G.y to the standard base point coordinates for secp256r1
    uint8_t gx_bytes[32] = {
        0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
        0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2,
        0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
        0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96
    };

    uint8_t gy_bytes[32] = {
        0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b,
        0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
        0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce,
        0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5
    };

    if (bigint_from_bytes(g->x, gx_bytes, sizeof(gx_bytes)) != -1 &&
        bigint_from_bytes(g->y, gy_bytes, sizeof(gy_bytes)) != -1 &&
        bigint_set_int64(g->z, 1) != -1) {
        return g;
    }

    ellipticcurve_jacobian_point_destroy(g);
    return NULL;
}

static ellipticcurve_point_t* ellipticcurve_convert_jacobian_to_affine(const ellipticcurve_jacobian_point_t* jacobian_point,
                                                                       const ellipticcurve_curve_t*          curve) {
    if (!jacobian_point || !curve) {
        return NULL;
    }

    if (ellipticcurve_jacobian_point_is_at_infinity(jacobian_point)) {
        return NULL; // Point at infinity has no affine representation
    }

    ellipticcurve_point_t* affine_point = NULL;
    bigint_t* z_inv = NULL;
    bigint_t* z_inv_squared = NULL;
    bigint_t* z_inv_cubed = NULL;

    // Compute Z_inv = Z^(p-2) mod p
    z_inv = bigint_create();
    z_inv_squared = bigint_create();
    z_inv_cubed = bigint_create();

    if (!z_inv || !z_inv_squared || !z_inv_cubed) {
        goto cleanup;
    }

    // Z_inv = Z^(p-2) mod p
    if(bigint_pow_mod(z_inv, jacobian_point->z, curve->p_minus_2, curve->p) == -1) {
        goto cleanup;
    }

    // Z_inv_squared = Z_inv^2 mod p
    if (bigint_mul_mod(z_inv_squared, z_inv, z_inv, curve->p) == -1) {
        goto cleanup;
    }

    // Z_inv_cubed = Z_inv^3 mod p
    if (bigint_mul_mod(z_inv_cubed, z_inv_squared, z_inv, curve->p) == -1) {
        goto cleanup;
    }

    affine_point = ellipticcurve_point_create();
    if (!affine_point) {
        goto cleanup;
    }

    // x_affine = X * Z_inv_squared mod p
    if (bigint_mul_mod(affine_point->x, jacobian_point->x, z_inv_squared, curve->p) == -1) {
        ellipticcurve_point_destroy(affine_point);
        affine_point = NULL;
        goto cleanup;
    }

    // y_affine = Y * Z_inv_cubed mod p
    if (bigint_mul_mod(affine_point->y, jacobian_point->y, z_inv_cubed, curve->p) == -1) {
        ellipticcurve_point_destroy(affine_point);
        affine_point = NULL;
        goto cleanup;
    }

cleanup:
    bigint_destroy(z_inv);
    bigint_destroy(z_inv_squared);
    bigint_destroy(z_inv_cubed);
    return affine_point;
}

static void ellipticcurve_curve_destroy(ellipticcurve_curve_t* curve) {
    if (!curve) {
        return;
    }

    if (curve->p) {
        bigint_destroy(curve->p);
    }
    if (curve->p_minus_2) {
        bigint_destroy(curve->p_minus_2);
    }
    if (curve->a) {
        bigint_destroy(curve->a);
    }
    if (curve->b) {
        bigint_destroy(curve->b);
    }
    if (curve->n) {
        bigint_destroy(curve->n);
    }
    if (curve->n_half) {
        bigint_destroy(curve->n_half);
    }
    if (curve->g) {
        ellipticcurve_jacobian_point_destroy(curve->g);
    }

    memory_free(curve);
}

static ellipticcurve_curve_t* ellipticcurve_secp256r1_create_curve() {
    ellipticcurve_curve_t* curve = memory_malloc(sizeof(ellipticcurve_curve_t));
    if (!curve) {
        return NULL;
    }

    // Initialize curve parameters for secp256r1
    uint8_t p_bytes[32] = {
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    uint8_t p_minus_2_bytes[32] = {
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd
    };

    uint8_t a_bytes[32] = {
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc
    };

    uint8_t b_bytes[32] = {
        0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7,
        0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc,
        0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53, 0xb0, 0xf6,
        0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b
    };

    uint8_t n_bytes[32] = {
        0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84,
        0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51
    };

    uint8_t n_half_bytes[32] = {
        0x7f, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
        0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xde, 0x73, 0x7d, 0x56, 0xd3, 0x8b, 0xcf, 0x42,
        0x79, 0xdc, 0xe5, 0x61, 0x7e, 0x31, 0x92, 0xa8
    };

    curve->p = bigint_create();
    curve->p_minus_2 = bigint_create();
    curve->a = bigint_create();
    curve->b = bigint_create();
    curve->n = bigint_create();
    curve->n_half = bigint_create();
    curve->g = ellipticcurve_secp256r1_create_g();

    if (!curve->p || !curve->p_minus_2 || !curve->a || !curve->b || !curve->n || !curve->n_half || !curve->g) {
        ellipticcurve_curve_destroy(curve);
        return NULL;
    }

    if (bigint_from_bytes(curve->p, p_bytes, sizeof(p_bytes)) != -1 &&
        bigint_from_bytes(curve->p_minus_2, p_minus_2_bytes, sizeof(p_minus_2_bytes)) != -1 &&
        bigint_from_bytes(curve->a, a_bytes, sizeof(a_bytes)) != -1 &&
        bigint_from_bytes(curve->b, b_bytes, sizeof(b_bytes)) != -1 &&
        bigint_from_bytes(curve->n, n_bytes, sizeof(n_bytes)) != -1 &&
        bigint_from_bytes(curve->n_half, n_half_bytes, sizeof(n_half_bytes)) != -1) {
        return curve;
    }

    ellipticcurve_curve_destroy(curve);
    return NULL;
}

static int8_t ellipticcurve_jacobian_point_set_to_infinity(ellipticcurve_jacobian_point_t* point) {
    if (!point) {
        return -1;
    }

    // Set Z to 0 to represent the point at infinity in Jacobian coordinates
    return bigint_set_zero(point->z);
}

static int8_t ellipticcurve_jacobian_point_copy(ellipticcurve_jacobian_point_t*       dest,
                                                const ellipticcurve_jacobian_point_t* src) {
    if (!dest || !src) {
        return -1;
    }

    if(bigint_set_bigint(dest->x, src->x) != 0 ||
       bigint_set_bigint(dest->y, src->y) != 0 ||
       bigint_set_bigint(dest->z, src->z) != 0) {
        return -1;
    }

    if (!dest->x || !dest->y || !dest->z) {
        return -1;
    }

    return 0;
}

static int8_t ellipticcurve_secp256r1_point_double(ellipticcurve_jacobian_point_t*       result,
                                                   const ellipticcurve_jacobian_point_t* point,
                                                   const ellipticcurve_curve_t*          curve) {
    if (!result || !point || !curve) {
        return -1;
    }

    // 1. Handle Point at Infinity: 2 * Inf = Inf
    if (bigint_is_zero(point->z)) {
        if(bigint_set_zero(result->x) == -1 ||
           bigint_set_zero(result->y) == -1 ||
           bigint_set_zero(result->z) == -1) {
            return -1;
        }
        return 0;
    }

    int8_t ret = -1;
    // Temporary variables for the Jacobian math
    bigint_t * t1 = bigint_create(); // M (slope)
    bigint_t * t2 = bigint_create(); // S
    bigint_t * t3 = bigint_create(); // T
    bigint_t * t4 = bigint_create(); // Y^2
    bigint_t* new_x = bigint_create(); // X3
    bigint_t* new_y = bigint_create(); // Y3
    bigint_t * p  = curve->p;

    if (!t1 || !t2 || !t3 || !t4 || !new_x || !new_y) {
        goto cleanup;
    }

    // --- Step 1: Calculate M = 3(X1 - Z1^2)(X1 + Z1^2) ---
    // Using the a = -3 optimization for P-256
    // t1 = Z^2
    if(bigint_mul_mod(t1, point->z, point->z, p) != 0) {
        goto cleanup;
    }
    // t2 = X - Z^2
    if(bigint_sub_mod(t2, point->x, t1, p) != 0) {
        goto cleanup;
    }
    // t1 = X + Z^2
    if(bigint_add_mod(t1, point->x, t1, p) != 0) {
        goto cleanup;
    }
    // t1 = (X+Z^2)(X-Z^2)
    if(bigint_mul_mod(t1, t1, t2, p) != 0) {
        goto cleanup;
    }
    // t1 = M = 3(X^2 - Z^4)
    if(bigint_set_uint64(t3, 3) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(t1, t1, t3, p) != 0) {
        goto cleanup;
    }

    // --- Step 2: Calculate S = 4 * X * Y^2 ---
    // t4 = Y^2
    if(bigint_mul_mod(t4, point->y, point->y, p) != 0) {
        goto cleanup;
    }
    // t2 = X * Y^2
    if(bigint_mul_mod(t2, point->x, t4, p) != 0) {
        goto cleanup;
    }
    // t2 = S = 4XY^2
    if(bigint_set_uint64(t3, 4) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(t2, t2, t3, p) != 0) {
        goto cleanup;
    }

    // --- Step 3: Calculate X3 = M^2 - 2S ---
    // X3 = M^2
    if(bigint_mul_mod(new_x, t1, t1, p) != 0) {
        goto cleanup;
    }
    // t3 = 2S
    if(bigint_add_mod(t3, t2, t2, p) != 0) {
        goto cleanup;
    }
    // X3 = M^2 - 2S
    if(bigint_sub_mod(new_x, new_x, t3, p) != 0) {
        goto cleanup;

    }
    // --- Step 4: Calculate Y3 = M(S - X3) - 8Y^4 ---
    // t3 = S - X3
    if(bigint_sub_mod(t3, t2, new_x, p) != 0) {
        goto cleanup;
    }
    // t3 = M(S - X3)
    if(bigint_mul_mod(t3, t1, t3, p) != 0) {
        goto cleanup;
    }
    // t4 = Y^4
    if(bigint_mul_mod(t4, t4, t4, p) != 0) {
        goto cleanup;
    }
    // t4 = 8Y^4
    if(bigint_set_uint64(t1, 8) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(t4, t4, t1, p) != 0) {
        goto cleanup;
    }
    // Y3 = M(S - X3) - 8Y^4
    if(bigint_sub_mod(new_y, t3, t4, p) != 0) {
        goto cleanup;
    }

    // --- Step 5: Calculate Z3 = 2 * Y * Z ---
    // Z3 = 2YZ
    if(bigint_mul_mod(result->z, point->y, point->z, p) != 0) {
        goto cleanup;
    }
    if(bigint_add_mod(result->z, result->z, result->z, p) != 0) {
        goto cleanup;
    }

    if(bigint_set_bigint(result->x, new_x) != 0) {
        goto cleanup;
    }

    if(bigint_set_bigint(result->y, new_y) != 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    bigint_destroy(t1);
    bigint_destroy(t2);
    bigint_destroy(t3);
    bigint_destroy(t4);
    bigint_destroy(new_x);
    bigint_destroy(new_y);
    return ret;
}

static int8_t ellipticcurve_secp256r1_point_add(ellipticcurve_jacobian_point_t*       result,
                                                const ellipticcurve_jacobian_point_t* p1,
                                                const ellipticcurve_jacobian_point_t* p2,
                                                const ellipticcurve_curve_t*          curve) {
    if (!result || !p1 || !p2 || !curve) {
        return -1;
    }

    // 1. Handle Points at Infinity
    if (bigint_is_zero(p1->z)) {
        return ellipticcurve_jacobian_point_copy(result, p2);
    }
    if (bigint_is_zero(p2->z)) {
        return ellipticcurve_jacobian_point_copy(result, p1);
    }

    int8_t ret = -1;
    bigint_t * u1 = bigint_create(), * u2 = bigint_create();
    bigint_t * s1 = bigint_create(), * s2 = bigint_create();
    bigint_t * h  = bigint_create(), * r  = bigint_create();
    bigint_t * t1 = bigint_create(), * t2 = bigint_create(), * t3 = bigint_create();
    bigint_t * new_x = bigint_create();
    bigint_t * p  = curve->p;

    if (!u1 || !u2 || !s1 || !s2 || !h || !r || !t1 || !t2 || !t3 || !new_x) {
        goto cleanup;
    }

    // --- Step 2: Calculate U1 = X1*Z2^2 and U2 = X2*Z1^2 ---
    if(bigint_mul_mod(t1, p2->z, p2->z, p) != 0) { // Z2^2
        goto cleanup;
    }
    if(bigint_mul_mod(u1, p1->x, t1, p) != 0) { // U1 = X1*Z2^2
        goto cleanup;

    }
    if(bigint_mul_mod(t2, p1->z, p1->z, p) != 0) { // Z1^2
        goto cleanup;
    }
    if(bigint_mul_mod(u2, p2->x, t2, p) != 0) { // U2 = X2*Z1^2
        goto cleanup;
    }

    // --- Step 3: Calculate S1 = Y1*Z2^3 and S2 = Y2*Z1^3 ---
    if(bigint_mul_mod(t1, t1, p2->z, p) != 0) { // Z2^3
        goto cleanup;
    }
    if(bigint_mul_mod(s1, p1->y, t1, p) != 0) { // S1 = Y1*Z2^3
        goto cleanup;

    }
    if(bigint_mul_mod(t2, t2, p1->z, p) != 0) { // Z1^3
        goto cleanup;
    }
    if(bigint_mul_mod(s2, p2->y, t2, p) != 0) { // S2 = Y2*Z1^3
        goto cleanup;
    }

    // --- Step 4: Check if points are the same or opposites ---
    if (bigint_cmp(u1, u2) == 0) {
        if (bigint_cmp(s1, s2) == 0) {
            // Points are equal: P1 + P1 = 2P1
            ret = ellipticcurve_secp256r1_point_double(result, p1, curve);
            goto cleanup;
        } else {
            // Points are opposites: P1 + (-P1) = Infinity
            if(bigint_set_zero(result->x) == -1 ||
               bigint_set_zero(result->y) == -1 ||
               bigint_set_zero(result->z) == -1) {
                goto cleanup;
            }
            ret = 0;
            goto cleanup;
        }
    }

    // --- Step 5: H = U2 - U1, R = S2 - S1 ---
    if(bigint_sub_mod(h, u2, u1, p) != 0) { // H = U2 - U1
        goto cleanup;
    }
    if(bigint_sub_mod(r, s2, s1, p) != 0) { // R = S2 - S1
        goto cleanup;
    }

    // --- Step 6: Calculate X3 = R^2 - H^3 - 2*U1*H^2 ---
    if(bigint_mul_mod(t1, h, h, p) != 0) { // H^2
        goto cleanup;
    }
    if(bigint_mul_mod(t2, t1, h, p) != 0) { // H^3
        goto cleanup;
    }
    if(bigint_mul_mod(t3, u1, t1, p) != 0) { // U1*H^2
        goto cleanup;
    }
    if(bigint_mul_mod(new_x, r, r, p) != 0) { // R^2
        goto cleanup;
    }
    if(bigint_sub_mod(new_x, new_x, t2, p) != 0) { // R^2 - H^3
        goto cleanup;
    }
    if(bigint_add_mod(t1, t3, t3, p) != 0) { // 2*U1*H^2 (recycle t1)
        goto cleanup;
    }
    if(bigint_sub_mod(new_x, new_x, t1, p) != 0) { // X3 = R^2 - H^3 - 2U1H^2
        goto cleanup;
    }

    // --- Step 7: Calculate Y3 = R(U1*H^2 - X3) - S1*H^3 ---

    // 7.1. Calculate (U1*H^2 - X3)
    // We use t3 (which is U1*H^2) and subtract the newly calculated X3 (stored in new_x)
    if(bigint_sub_mod(t1, t3, new_x, p) != 0) {
        goto cleanup;
    }

    // 7.2. Multiply by R
    if(bigint_mul_mod(result->y, r, t1, p) != 0) {
        goto cleanup;
    }

    // 7.3. Calculate S1 * H^3
    // We still have H^3 stored in t2 from Step 6
    if(bigint_mul_mod(t2, s1, t2, p) != 0) {
        goto cleanup;
    }

    // 7.4. Final subtraction: Y3 = [R(U1H^2 - X3)] - [S1H^3]
    if(bigint_sub_mod(result->y, result->y, t2, p) != 0) {
        goto cleanup;
    }

    // --- Step 8: Calculate Z3 = H*Z1*Z2 ---
    if(bigint_mul_mod(result->z, p1->z, p2->z, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(result->z, result->z, h, p) != 0) { // Z3 = H*Z1*Z2
        goto cleanup;
    }

    if(bigint_set_bigint(result->x, new_x) != 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    bigint_destroy(u1); bigint_destroy(u2);
    bigint_destroy(s1); bigint_destroy(s2);
    bigint_destroy(h);  bigint_destroy(r);
    bigint_destroy(t1); bigint_destroy(t2); bigint_destroy(t3);
    bigint_destroy(new_x);
    return ret;
}

static int8_t ellipticcurve_secp256r1_scalar_mult(ellipticcurve_jacobian_point_t*       result,
                                                  const bigint_t*                       scalar,
                                                  const ellipticcurve_jacobian_point_t* point,
                                                  const ellipticcurve_curve_t*          curve) {
    if (!result || !scalar || !point || !curve) {
        return -1;
    }

    ellipticcurve_jacobian_point_t* temp = NULL;

    int8_t err = -1;

    if (ellipticcurve_jacobian_point_set_to_infinity(result) != 0) {
        goto cleanup;
    }

    temp = ellipticcurve_jacobian_point_create();
    if (!temp) {
        goto cleanup;
    }

    if (ellipticcurve_jacobian_point_copy(temp, point) != 0) {
        goto cleanup;
    }

    for (int32_t i = 0; i < 256; i++) {
        boolean_t bit;

        if (bigint_get_bit(scalar, i, &bit) != 0) {
            goto cleanup;
        }

        if (bit) {
            if (ellipticcurve_secp256r1_point_add(result, result, temp, curve) != 0) {
                goto cleanup;
            }
        }

        if (ellipticcurve_secp256r1_point_double(temp, temp, curve) != 0) {
            goto cleanup;
        }
    }

    err = 0; // Success

cleanup:
    ellipticcurve_jacobian_point_destroy(temp);
    return err;
}

static int8_t ellipticcurve_secp256r1_scalar_mult_g(ellipticcurve_jacobian_point_t* result,
                                                    const bigint_t*                 scalar,
                                                    const ellipticcurve_curve_t*    curve) {
    if (!result || !scalar || !curve) {
        return -1;
    }

    if(ellipticcurve_secp256r1_scalar_mult(result, scalar, curve->g, curve) != 0) {
        return -1;
    }
    return 0;
}


int8_t ellipticcurve_secp256r1_derive_pubkey(uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN],
                                             const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]) {

    bigint_t* private_scalar = NULL;
    ellipticcurve_jacobian_point_t* temp = NULL;
    ellipticcurve_jacobian_point_t* res = NULL;
    ellipticcurve_point_t* affine_res = NULL;
    ellipticcurve_curve_t* curve = NULL;

    int8_t err = -1;

    if (!priv || !out_pub) {
        return -1;
    }

    curve = ellipticcurve_secp256r1_create_curve();
    if (!curve) {
        goto cleanup;
    }

    private_scalar = bigint_create();

    if (!private_scalar) {
        goto cleanup;
    }

    if(bigint_from_bytes(private_scalar, priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) != 0) {
        goto cleanup;
    }

    temp = ellipticcurve_secp256r1_create_g();
    if (!temp) {
        goto cleanup;
    }

    res = ellipticcurve_create_jacobian_point_at_infinity();
    if (!res) {
        goto cleanup;
    }

    for(int32_t i = 0; i < 256; i++) {
        boolean_t bit;

        if(bigint_get_bit(private_scalar, i, &bit) != 0) {
            goto cleanup;
        }

        if (bit) {
            if(ellipticcurve_secp256r1_point_add(res, res, temp, curve) != 0) {
                goto cleanup;
            }
        }

        if(ellipticcurve_secp256r1_point_double(temp, temp, curve) != 0) {
            goto cleanup;
        }
    }

    affine_res = ellipticcurve_convert_jacobian_to_affine(res, curve);
    if (!affine_res) {
        goto cleanup;
    }

    uint8_t x_bytes[32];
    uint8_t y_bytes[32];

    if (bigint_to_bytes(affine_res->x, x_bytes, sizeof(x_bytes)) != 0 ||
        bigint_to_bytes(affine_res->y, y_bytes, sizeof(y_bytes)) != 0) {
        goto cleanup;
    }

    // Output format: [X (32 bytes) || Y (32 bytes)]
    memory_memcopy(x_bytes, out_pub, sizeof(x_bytes));
    memory_memcopy(y_bytes, out_pub + sizeof(x_bytes), sizeof(y_bytes));

    err = 0; // Success
cleanup:
    bigint_destroy(private_scalar);
    ellipticcurve_jacobian_point_destroy(temp);
    ellipticcurve_jacobian_point_destroy(res);
    ellipticcurve_point_destroy(affine_res);
    ellipticcurve_curve_destroy(curve);
    return err;
}

int8_t ellipticcurve_secp256r1_generate_keypair(uint8_t out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                                uint8_t out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]) {
    uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];

    get_random_bytes(priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN);

    if(ellipticcurve_secp256r1_derive_pubkey(out_pub, priv) != 0) {
        memory_memclean(priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN); // Clear temporary private key buffer
        return -1;
    }

    memory_memcopy(priv, out_priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN);
    memory_memclean(priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN); // Clear temporary private key buffer
    return 0;
}

int8_t ellipticcurve_secp256r1_sign(uint8_t out_sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                    const uint8_t* msg, size_t msg_len,
                                    const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]) {
    if (!priv || !msg || msg_len == 0 || !out_sig) {
        return -1;
    }

    int8_t ret = -1;
    bigint_t * d = NULL, * e = NULL, * k = NULL, * r = NULL, * s = NULL, * k_inv = NULL, * tmp = NULL;
    ellipticcurve_jacobian_point_t* R_jac = NULL;
    ellipticcurve_point_t* R_aff = NULL;
    uint8_t hash[SHA256_OUTPUT_SIZE];
    ellipticcurve_curve_t* curve = NULL;

    // 1. Hash the message (SHA-256)
    uint8_t* hash_result = sha256_hash(msg, msg_len);
    if (!hash_result) {
        return -1;
    }
    memory_memcopy(hash_result, hash, SHA256_OUTPUT_SIZE);
    memory_free(hash_result);

    // 2. Create BigInts
    d = bigint_create();
    if (!d) {
        goto cleanup;
    }
    if(bigint_from_bytes(d, priv, SHA256_OUTPUT_SIZE) != 0) {
        goto cleanup;
    }

    e = bigint_create();
    if (!e) {
        goto cleanup;
    }
    if(bigint_from_bytes(e, hash, SHA256_OUTPUT_SIZE) != 0) {
        goto cleanup;
    }

    k = bigint_create();
    r = bigint_create();
    s = bigint_create();
    k_inv = bigint_create();
    tmp = bigint_create();

    if (!d || !e || !k || !r || !s || !k_inv || !tmp) {
        goto cleanup;
    }

    curve = ellipticcurve_secp256r1_create_curve();
    if (!curve) {
        goto cleanup;
    }

    R_jac = ellipticcurve_create_jacobian_point_at_infinity();
    if (!R_jac) {
        goto cleanup;
    }

    // 3. Signing Loop (in case r or s result in 0)
    do {
        // Step A: Generate random k where 1 <= k < n
        do {
            get_random_bytes(hash, 32); // Reuse hash buffer for random bits
            if (bigint_from_bytes(k, hash, 32) != 0) {
                goto cleanup;
            }
        } while (bigint_is_zero(k) || bigint_cmp(k, curve->n) >= 0);

        // Step B: Compute R = k * G
        if(ellipticcurve_secp256r1_scalar_mult_g(R_jac, k, curve) != 0) {
            goto cleanup;
        }

        // Step C: Convert R to Affine to get x-coordinate
        R_aff = ellipticcurve_convert_jacobian_to_affine(R_jac, curve);
        if (!R_aff) {
            ellipticcurve_jacobian_point_destroy(R_jac);
            goto cleanup;
        }

        // Step D: r = x_R mod n
        if(bigint_mod(r, R_aff->x, curve->n) != 0) {
            goto cleanup;
        }

        ellipticcurve_point_destroy(R_aff);
        R_aff = NULL;

        if (bigint_is_zero(r)) {
            continue;
        }

        // Step E: s = k^-1 * (e + r*d) mod n
        // Calculate k_inv = k^(n-2) mod n (or Extended Euclidean)
        if (bigint_mod_inv(k_inv, k, curve->n) != 0) {
            goto cleanup;
        }

        if(bigint_mul_mod(tmp, r, d, curve->n) != 0) { // tmp = r*d mod n
            goto cleanup;
        }
        if(bigint_add_mod(tmp, e, tmp, curve->n) != 0) { // tmp = e + r*d mod n
            goto cleanup;
        }
        if(bigint_mul_mod(s, k_inv, tmp, curve->n) != 0) { // s = k_inv * tmp mod n
            goto cleanup;
        }
    } while (bigint_is_zero(s));

    // 4. Export r and s to output buffer
    if(bigint_to_bytes(r, out_sig, 32) != 0) {
        goto cleanup;
    }

    if(bigint_cmp(s, curve->n_half) > 0) {
        // If s > n/2, then s = n - s (to enforce low S values)
        if(bigint_sub_mod(s, curve->n, s, curve->n) != 0) {
            goto cleanup;
        }
    }

    if(bigint_to_bytes(s, out_sig + 32, 32) != 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    bigint_destroy(d); bigint_destroy(e); bigint_destroy(k);
    bigint_destroy(r); bigint_destroy(s); bigint_destroy(k_inv);
    bigint_destroy(tmp);
    ellipticcurve_jacobian_point_destroy(R_jac);
    ellipticcurve_point_destroy(R_aff);
    ellipticcurve_curve_destroy(curve);
    return ret;
}

int8_t ellipticcurve_secp256r1_verify(const uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN],
                                      const uint8_t* msg, size_t msg_len,
                                      const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]) {
    if (!sig || !msg || msg_len == 0 || !pub) {
        return -1;
    }

    int8_t ret = -1;
    bigint_t * r = NULL, * s = NULL, * e = NULL, * w = NULL, * u1 = NULL, * u2 = NULL, * v = NULL;
    ellipticcurve_point_t * R_aff = NULL;
    ellipticcurve_jacobian_point_t * u1G = NULL, * u2Q = NULL, * R_jac = NULL, * Q_jac = NULL;
    ellipticcurve_curve_t* curve = NULL;

    curve = ellipticcurve_secp256r1_create_curve();
    if (!curve) {
        goto cleanup;
    }

    // 1. Decode r and s from signature
    r = bigint_create();
    s = bigint_create();
    if (!r || !s) {
        goto cleanup;
    }
    if(bigint_from_bytes(r, sig, 32) != 0) {
        goto cleanup;
    }
    if(bigint_from_bytes(s, sig + 32, 32) != 0) {
        goto cleanup;
    }

    // 2. Validate r and s are in range [1, n-1]
    if (bigint_is_zero(r) || bigint_cmp(r, curve->n) >= 0 ||
        bigint_is_zero(s) || bigint_cmp(s, curve->n) >= 0) {
        goto cleanup; // Invalid signature values
    }

    // 3. Hash the message (SHA-256)
    uint8_t* hash_result = sha256_hash(msg, msg_len);
    if (!hash_result) {
        goto cleanup;
    }
    e = bigint_create();
    if (!e) {
        memory_free(hash_result);
        goto cleanup;
    }
    if(bigint_from_bytes(e, hash_result, SHA256_OUTPUT_SIZE) != 0) {
        memory_free(hash_result);
        goto cleanup;
    }
    memory_free(hash_result);

    // 4. Verification Math (Modulo the Order n)
    w = bigint_create();
    u1 = bigint_create();
    u2 = bigint_create();
    if (!w || !u1 || !u2) {
        goto cleanup;
    }

    // w = s^-1 mod n
    if (bigint_mod_inv(w, s, curve->n) != 0) {
        goto cleanup;
    }

    // u1 = e * w mod n
    if(bigint_mul_mod(u1, e, w, curve->n) != 0) {
        goto cleanup;
    }

    // u2 = r * w mod n
    if(bigint_mul_mod(u2, r, w, curve->n) != 0) {
        goto cleanup;
    }

    // 5. Curve Math (Modulo the Prime p)
    Q_jac = ellipticcurve_jacobian_point_create_from_bytes(pub, pub + 32);
    if (!Q_jac) {
        goto cleanup;
    }

    // R = u1*G + u2*Q
    u1G = ellipticcurve_create_jacobian_point_at_infinity();
    if(ellipticcurve_secp256r1_scalar_mult_g(u1G, u1, curve) != 0) { // Point 1
        goto cleanup;
    }
    u2Q = ellipticcurve_create_jacobian_point_at_infinity();
    if(ellipticcurve_secp256r1_scalar_mult(u2Q, u2, Q_jac, curve) != 0) { // Point 2
        goto cleanup;
    }

    // R = u1G + u2Q (Point Addition in Jacobian)
    R_jac = ellipticcurve_create_jacobian_point_at_infinity();
    if(ellipticcurve_secp256r1_point_add(R_jac, u1G, u2Q, curve) != 0) {
        goto cleanup;
    }
    if (ellipticcurve_jacobian_point_is_at_infinity(R_jac)) {
        goto cleanup;
    }

    // 6. Final Comparison
    R_aff = ellipticcurve_convert_jacobian_to_affine(R_jac, curve);
    if (!R_aff) {
        goto cleanup;
    }

    // v = x_R mod n
    v = bigint_create();
    if(bigint_mod(v, R_aff->x, curve->n) != 0) {
        goto cleanup;
    }

    // Check if v == r
    if (bigint_cmp(v, r) == 0) {
        ret = 0; // Signature is VALID
    } else {
        ret = 1; // Signature is INVALID
    }

cleanup:
    // Comprehensive cleanup of all BigInts and Points
    bigint_destroy(r); bigint_destroy(s); bigint_destroy(e);
    bigint_destroy(w); bigint_destroy(u1); bigint_destroy(u2);
    bigint_destroy(v);
    ellipticcurve_point_destroy(R_aff);
    ellipticcurve_jacobian_point_destroy(u1G);
    ellipticcurve_jacobian_point_destroy(u2Q);
    ellipticcurve_jacobian_point_destroy(R_jac);
    ellipticcurve_jacobian_point_destroy(Q_jac);
    ellipticcurve_curve_destroy(curve);
    return ret;
}

int8_t ellipticcurve_secp256r1_shared_secret(uint8_t       shared_secret[ELLIPTICCURVE_SECP256R1_SHARED_SECRET_LEN],
                                             const uint8_t priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                             const uint8_t pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]) {
    if (!shared_secret || !priv || !pub) {
        return -1;
    }

    int8_t ret = -1;
    bigint_t* private_scalar = NULL;
    ellipticcurve_jacobian_point_t* temp = NULL;
    ellipticcurve_jacobian_point_t* res = NULL;
    ellipticcurve_point_t* affine_res = NULL;
    ellipticcurve_curve_t* curve = NULL;


    curve = ellipticcurve_secp256r1_create_curve();
    if (!curve) {
        goto cleanup;
    }

    private_scalar = bigint_create();
    if (!private_scalar) {
        goto cleanup;
    }

    if(bigint_from_bytes(private_scalar, priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) != 0) {
        goto cleanup;
    }

    temp = ellipticcurve_jacobian_point_create_from_bytes(pub, pub + 32);
    if (!temp) {
        goto cleanup;
    }

    res = ellipticcurve_create_jacobian_point_at_infinity();
    if (!res) {
        goto cleanup;
    }

    if(ellipticcurve_secp256r1_scalar_mult(res, private_scalar, temp, curve) != 0) {
        goto cleanup;
    }

    affine_res = ellipticcurve_convert_jacobian_to_affine(res, curve);
    if (!affine_res) {
        goto cleanup;
    }

    uint8_t x_bytes[32];
    if (bigint_to_bytes(affine_res->x, x_bytes, sizeof(x_bytes)) != 0) {
        goto cleanup;
    }

    memory_memcopy(x_bytes, shared_secret, sizeof(x_bytes));
    ret = 0; // Success
cleanup:
    bigint_destroy(private_scalar);
    ellipticcurve_jacobian_point_destroy(temp);
    ellipticcurve_jacobian_point_destroy(res);
    ellipticcurve_point_destroy(affine_res);
    ellipticcurve_curve_destroy(curve);
    return ret;
}

int8_t pem_read_secp256r1_private_key(const char_t* pem,
                                      uint8_t       out_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN]) {
    if (!pem || !out_priv) {
        return -1;
    }

    uint8_t* der_data = NULL;
    size_t der_len = 0;
    if (pem_decode("EC PRIVATE KEY", pem, strlen(pem), &der_data, &der_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode PEM");
        return -1;
    }

    der_decoder_t* decoder = der_decoder_new(der_data, der_len);
    if (!decoder) {
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create DER decoder");
        return -1;
    }

    // Parse the DER structure to extract the private key
    // Expected structure:
    // SEQ {
    // INTEGER 1 (Version)
    // OCTET STRING (Private Key)
    // [0] EXPLICIT { OBJECT IDENTIFIER secp256r1 }
    // [1] EXPLICIT { BIT STRING (Public Key) }
    // }

    if (der_decoder_start_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER sequence");
        return -1;
    }

    int64_t version;
    if (der_decoder_decode_integer(decoder, &version) != 0 || version != 1) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER version");
        return -1;
    }

    uint8_t* priv_key_data = NULL;
    size_t priv_key_len = 0;
    if (der_decoder_decode_octet_string(decoder, &priv_key_data, &priv_key_len) != 0 || priv_key_len != ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER private key");
        return -1;
    }

    if (der_decoder_start_explicit_tag(decoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 0) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER explicit tag for OID");
        return -1;
    }

    der_object_identifier_t oid;
    if (der_decoder_decode_object_identifier(decoder, &oid) != 0 || oid != DER_OID_EC_SECP256R1) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER OID for secp256r1");
        return -1;
    }

    if (der_decoder_end_explicit_tag(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER explicit tag for OID");
        return -1;
    }

    if (der_decoder_start_explicit_tag(decoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 1) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER explicit tag for public key");
        return -1;
    }

    uint8_t* pub_key_data = NULL;
    size_t pub_key_len = 0;
    if (der_decoder_decode_bit_string(decoder, &pub_key_data, &pub_key_len) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode DER bit string for public key");
        return -1;
    }

    if (pub_key_len != 1 + ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN || pub_key_data[0] != 0x04) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        memory_free(pub_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER public key format");
        return -1;
    }

    if (der_decoder_end_explicit_tag(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        memory_free(pub_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER explicit tag for public key");
        return -1;
    }

    uint8_t derived_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    if (ellipticcurve_secp256r1_derive_pubkey(derived_pub, priv_key_data) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        memory_free(pub_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to derive public key from private key");
        return -1;
    }

    if (memory_memcompare(derived_pub, pub_key_data + 1, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(priv_key_data);
        memory_free(pub_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Public key in DER does not match derived public key from private key");
        return -1;
    }

    memory_memcopy(priv_key_data, out_priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN);

    der_decoder_destroy(decoder);
    memory_free(der_data);
    memory_free(priv_key_data);
    memory_free(pub_key_data);
    return 0;
}

int8_t pem_read_secp256r1_public_key(const char_t* pem,
                                     uint8_t       out_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN]) {
    if (!pem || !out_pub) {
        return -1;
    }

    uint8_t* der_data = NULL;
    size_t der_len = 0;
    if (pem_decode("PUBLIC KEY", pem, strlen(pem), &der_data, &der_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode PEM");
        return -1;
    }

    der_decoder_t* decoder = der_decoder_new(der_data, der_len);
    if (!decoder) {
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create DER decoder");
        return -1;
    }

    // Parse the DER structure to extract the public key
    // Expected structure:
    // SEQ {
    // SEQ {
    // OBJECT IDENTIFIER (id-ecPublicKey)
    // OBJECT IDENTIFIER (secp256r1)
    // }
    // BIT STRING (Public Key)
    // }

    if (der_decoder_start_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER sequence");
        return -1;
    }

    if (der_decoder_start_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER sequence for OIDs");
        return -1;
    }

    der_object_identifier_t oid;
    if (der_decoder_decode_object_identifier(decoder, &oid) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode DER OID for public key");
        return -1;
    }

    if(oid != DER_OID_ECDSA_PUBLIC_KEY) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER OID for public key (expected id-ecPublicKey)");
        return -1;
    }

    if (der_decoder_decode_object_identifier(decoder, &oid) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode DER OID for curve");
        return -1;
    }

    if(oid != DER_OID_EC_SECP256R1) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER OID for curve (expected secp256r1)");
        return -1;
    }

    if (der_decoder_end_sequence(decoder) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER sequence for OIDs");
        return -1;
    }

    uint8_t* pub_key_data = NULL;
    size_t pub_key_len = 0;
    if (der_decoder_decode_bit_string(decoder, &pub_key_data, &pub_key_len) != 0) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode DER bit string for public key");
        return -1;
    }

    if (pub_key_len != 1 + ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN || pub_key_data[0] != 0x04) {
        der_decoder_destroy(decoder);
        memory_free(der_data);
        memory_free(pub_key_data);
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER public key format");
        return -1;
    }

    memory_memcopy(pub_key_data + 1, out_pub, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN);

    der_decoder_destroy(decoder);
    memory_free(der_data);
    memory_free(pub_key_data);
    return 0;
}

int8_t pem_write_secp256r1_private_key(const uint8_t in_priv[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN],
                                       char_t**      out_pem) {
    if (!in_priv || !out_pem) {
        return -1;
    }

    uint8_t pub_key[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    if (ellipticcurve_secp256r1_derive_pubkey(pub_key, in_priv) != 0) {
        return -1;
    }

    der_encoder_t* encoder = der_encoder_new();
    if (!encoder) {
        return -1;
    }

    // SEQ {
    if (der_encoder_start_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // INTEGER 1 (Version)
    if (der_encoder_encode_integer(encoder, 1) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // OCTET STRING (Private Key)
    if (der_encoder_encode_octet_string(encoder, in_priv, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // [0] EXPLICIT { OBJECT IDENTIFIER secp256r1 }
    if (der_encoder_start_explicit_tag(encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 0) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }
    if(der_encoder_encode_object_identifier(encoder, DER_OID_EC_SECP256R1) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }
    if (der_encoder_end_explicit_tag(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // [1] EXPLICIT { BIT STRING (Public Key) }
    if (der_encoder_start_explicit_tag(encoder, DER_TAG_CLASS_CONTEXT_SPECIFIC, 1) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // Uncompressed point prefix 0x04
    uint8_t uncompressed_pub[1 + ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    uncompressed_pub[0] = 0x04; // Uncompressed point indicator
    memory_memcopy(pub_key, uncompressed_pub + 1, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN);
    if (der_encoder_encode_bit_string(encoder, uncompressed_pub, sizeof(uncompressed_pub)) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    if (der_encoder_end_explicit_tag(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // } SEQ
    if (der_encoder_end_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // Get DER bytes
    uint8_t* der_data = NULL;
    size_t der_len = 0;
    if (der_encoder_get_der_data(encoder, &der_data, &der_len) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    der_encoder_destroy(encoder);

    // PEM encode
    char_t* pem_data = NULL;
    size_t pem_len = 0;
    if (pem_encode("EC PRIVATE KEY", der_data, der_len, &pem_data, &pem_len) != 0) {
        memory_free(der_data);
        return -1;
    }

    *out_pem = pem_data;

    // Cleanup
    memory_free(der_data);
    return 0;
}

int8_t pem_write_secp256r1_public_key(const uint8_t in_pub[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN],
                                      char_t**      out_pem) {
    if (!in_pub || !out_pem) {
        return -1;
    }

    der_encoder_t* encoder = der_encoder_new();
    if (!encoder) {
        return -1;
    }

    // SEQ {
    if (der_encoder_start_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // SEQ {
    if (der_encoder_start_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // id-ecPublicKey OID
    if(der_encoder_encode_object_identifier(encoder, DER_OID_ECDSA_PUBLIC_KEY) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // secp256r1 OID
    if(der_encoder_encode_object_identifier(encoder, DER_OID_EC_SECP256R1) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // } SEQ
    if (der_encoder_end_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // BIT STRING {
    // Uncompressed point prefix 0x04
    uint8_t uncompressed_pub[1 + ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    uncompressed_pub[0] = 0x04; // Uncompressed point indicator
    memory_memcopy(in_pub, uncompressed_pub + 1, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN);
    if (der_encoder_encode_bit_string(encoder, uncompressed_pub, sizeof(uncompressed_pub)) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }
    // } BIT STRING

    // } SEQ
    if (der_encoder_end_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    // Get DER bytes
    uint8_t* der_data = NULL;
    size_t der_len = 0;
    if (der_encoder_get_der_data(encoder, &der_data, &der_len) != 0) {
        der_encoder_destroy(encoder);
        return -1;
    }

    der_encoder_destroy(encoder);

    // PEM encode
    char_t* pem_data = NULL;
    size_t pem_len = 0;
    if (pem_encode("PUBLIC KEY", der_data, der_len, &pem_data, &pem_len) != 0) {
        memory_free(der_data);
        return -1;
    }

    *out_pem = pem_data;

    // Cleanup
    memory_free(der_data);
    return 0;
}

uint8_t* ellipticcurve_secp256r1_encode_signature(const uint8_t sig[ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN], size_t* encoded_length) {
    if(!sig || !encoded_length) {
        return NULL;
    }

    der_encoder_t* encoder = der_encoder_new();
    if(!encoder) {
        return NULL;
    }

    if(der_encoder_start_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return NULL;
    }

    if(der_encoder_encode_integer_u256(encoder, sig) != 0) {
        der_encoder_destroy(encoder);
        return NULL;
    }

    if(der_encoder_encode_integer_u256(encoder, sig + 32) != 0) {
        der_encoder_destroy(encoder);
        return NULL;
    }

    if(der_encoder_end_sequence(encoder) != 0) {
        der_encoder_destroy(encoder);
        return NULL;
    }

    uint8_t* der_data = NULL;
    if(der_encoder_get_der_data(encoder, &der_data, encoded_length) != 0) {
        der_encoder_destroy(encoder);
        return NULL;
    }

    der_encoder_destroy(encoder);


    return der_data;
}
