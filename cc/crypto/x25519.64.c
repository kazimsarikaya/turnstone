/**
 * @file x25519.64.c
 * @brief X25519 Key Exchange Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/x25519.h>
#include <bigint.h>
#include <memory.h>
#include <random.h>
#include <logging.h>
#include <strings.h>
#include <base64.h>
#include <crypto/sha2.h>
#include <crypto/pem.h>
#include <crypto/der.h>

MODULE("turnstone.lib.crypto");

int8_t x25519_generate_keypair(uint8_t out_priv[X25519_PRIVATE_KEY_RAW_LEN], uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN]) {
    // 1. Get 32 random bytes
    get_random_bytes(out_priv, X25519_PRIVATE_KEY_RAW_LEN);

    // 2. Derive the public key
    // (The derivation function handles clamping internally)
    if (x25519_derive_public(out_pub, out_priv) != 0) {
        return -1;
    }

    return 0;
}

static int8_t x25519_scalarmult(uint8_t out[32], const uint8_t scalar[32], const bigint_t* u_base) {
    int8_t res = -1;
    bigint_t * p = NULL, * x0 = NULL, * z0 = NULL, * x1 = NULL, * z1 = NULL;
    bigint_t * da = NULL, * db = NULL, * dc = NULL, * dd = NULL;
    bigint_t * e = NULL, * f = NULL, * g = NULL, * h = NULL, * inv_z0 = NULL;

    const uint64_t a24 = 121666;

    // 1. Initialize Modulus and State
    p = bigint_create();
    if (bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto cleanup;
    }

    x0 = bigint_one();
    z0 = bigint_zero();
    x1 = bigint_clone(u_base);
    z1 = bigint_one();

    // 2. Initialize Temporaries
    da = bigint_create(); db = bigint_create(); dc = bigint_create(); dd = bigint_create();
    e = bigint_create(); f = bigint_create(); g = bigint_create(); h = bigint_create();
    if (!h) {
        goto cleanup; // Check last allocation

    }

    uint8_t prev_bit = 0;

    // 3. The Montgomery Ladder
    for (int64_t i = 254; i >= 0; i--) {
        uint8_t bit = (scalar[i >> 3] >> (i & 7)) & 1;

        uint8_t swap = bit ^ prev_bit;

        if(bigint_cswap(x0, x1, swap) != 0) {
            goto cleanup;
        }

        if(bigint_cswap(z0, z1, swap) != 0) {
            goto cleanup;
        }

        prev_bit = bit;

        // Formulas
        if (bigint_add_mod(da, x0, z0, p) != 0) {
            goto cleanup;
        }
        if (bigint_sub_mod(db, x0, z0, p) != 0) {
            goto cleanup;
        }
        if (bigint_add_mod(dc, x1, z1, p) != 0) {
            goto cleanup;
        }
        if (bigint_sub_mod(dd, x1, z1, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(e, dd, da, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(f, dc, db, p) != 0) {
            goto cleanup;
        }

        // x1 = (e + f)^2
        if (bigint_add_mod(g, e, f, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(x1, g, g, p) != 0) {
            goto cleanup;
        }

        // z1 = u_base * (e - f)^2
        if (bigint_sub_mod(g, e, f, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(h, g, g, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(z1, h, u_base, p) != 0) {
            goto cleanup;
        }

        // AA = da^2, BB = db^2, E = AA - BB
        if (bigint_mul_mod(e, da, da, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(f, db, db, p) != 0) {
            goto cleanup;
        }
        if (bigint_sub_mod(g, e, f, p) != 0) {
            goto cleanup;
        }

        // x0 = AA * BB
        if (bigint_mul_mod(x0, e, f, p) != 0) {
            goto cleanup;
        }

        // z0 = E * (BB + a24 * E)
        if (bigint_set_bigint(h, g) != 0) {
            goto cleanup;
        }
        if (bigint_mul_uint64(h, a24) != 0) {
            goto cleanup;
        }
        if (bigint_mod(h, h, p) != 0) {
            goto cleanup;
        }
        if (bigint_add_mod(h, h, f, p) != 0) {
            goto cleanup;
        }
        if (bigint_mul_mod(z0, g, h, p) != 0) {
            goto cleanup;
        }
    }

    if(bigint_cswap(x0, x1, prev_bit) != 0) {
        goto cleanup;
    }

    if(bigint_cswap(z0, z1, prev_bit) != 0) {
        goto cleanup;
    }

    // 4. Finalize: x = x0 * inv(z0)
    inv_z0 = bigint_create();
    if (bigint_mod_inv(inv_z0, z0, p) == 0) {
        if (bigint_mul_mod(x0, x0, inv_z0, p) != 0) {
            goto cleanup;
        }
        if (bigint_to_bytes_le(x0, out, 32) != 0) {
            goto cleanup;
        }
    } else {
        // Point at infinity/Zero
        memory_memset(out, 0, 32);
    }

    res = 0;

cleanup:
    bigint_destroy(p); bigint_destroy(x0); bigint_destroy(z0);
    bigint_destroy(x1); bigint_destroy(z1); bigint_destroy(da);
    bigint_destroy(db); bigint_destroy(dc); bigint_destroy(dd);
    bigint_destroy(e); bigint_destroy(f); bigint_destroy(g);
    bigint_destroy(h); bigint_destroy(inv_z0);
    return res;
}

int8_t x25519_derive_public(uint8_t out_pub[32], const uint8_t in_priv[32]) {
    uint8_t k[32];
    memory_memcopy(in_priv, k, 32);
    x25519_clamp(k);

    bigint_t* u_base = bigint_create();
    if(bigint_set_uint64(u_base, 9) != 0) {
        bigint_destroy(u_base);
        return -1;
    }

    int8_t res = x25519_scalarmult(out_pub, k, u_base);

    bigint_destroy(u_base);
    return res;
}

int8_t x25519_shared_secret(uint8_t out_shared[32], const uint8_t my_priv[32], const uint8_t their_pub[32]) {
    uint8_t k[32];
    memory_memcopy(my_priv, k, 32);
    x25519_clamp(k);

    bigint_t* u_peer = bigint_create();
    if (bigint_from_bytes_le(u_peer, their_pub, 32) != 0) {
        bigint_destroy(u_peer);
        return -1;
    }

    int8_t res = x25519_scalarmult(out_shared, k, u_peer);

    bigint_destroy(u_peer);
    return res;
}

void x25519_clamp(uint8_t k[X25519_PRIVATE_KEY_RAW_LEN]) {
    k[0] &= 248; // Clear bits 0, 1, 2
    k[31] &= 127; // Clear bit 255
    k[31] |= 64; // Set bit 254
}

typedef struct ed25519_point_t {
    bigint_t* X;
    bigint_t* Y;
    bigint_t* Z;
    bigint_t* T;
} ed25519_point_t;

static int8_t ed25519_point_add(ed25519_point_t* res, const ed25519_point_t* p1, const ed25519_point_t* p2, const bigint_t* p) {
    if (!res || !p1 || !p2 || !p) {
        return -1;
    }

    int8_t err = -1;

    // Temporary variables for the Hisil et al. formula
    bigint_t * YpX1 = bigint_create(), * YmX1 = bigint_create();
    bigint_t * YpX2 = bigint_create(), * YmX2 = bigint_create();
    bigint_t * A = bigint_create(), * B = bigint_create(), * C = bigint_create();
    bigint_t * D = bigint_create(), * E = bigint_create(), * F = bigint_create();
    bigint_t * G = bigint_create(), * H = bigint_create();
    bigint_t * d2 = bigint_create();

    // d2 = 2 * d mod p
    if(bigint_set_str(d2, "2406D9DC56DFFCE7198E80F2EEF3D13000E0149A8283B156EBD69B9426B2F159") != 0) {
        goto cleanup;
    }

    // A = (Y1-X1)*(Y2-X2)
    if(bigint_sub_mod(YmX1, p1->Y, p1->X, p) != 0) {
        goto cleanup;
    }
    if(bigint_sub_mod(YmX2, p2->Y, p2->X, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(A, YmX1, YmX2, p) != 0) {
        goto cleanup;
    }

    // B = (Y1+X1)*(Y2+X2)
    if(bigint_add_mod(YpX1, p1->Y, p1->X, p) != 0) {
        goto cleanup;
    }
    if(bigint_add_mod(YpX2, p2->Y, p2->X, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(B, YpX1, YpX2, p) != 0) {
        goto cleanup;
    }

    // C = T1*2d*T2
    if(bigint_mul_mod(C, p1->T, p2->T, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(C, C, d2, p) != 0) {
        goto cleanup;
    }

    // D = Z1*2*Z2
    if(bigint_mul_mod(D, p1->Z, p2->Z, p) != 0) {
        goto cleanup;
    }
    if(bigint_add_mod(D, D, D, p) != 0) { // D = 2 * Z1 * Z2
        goto cleanup;
    }

    // Intermediate steps
    if(bigint_sub_mod(E, B, A, p) != 0) { // E = B - A
        goto cleanup;
    }
    if(bigint_sub_mod(F, D, C, p) != 0) { // F = D - C
        goto cleanup;
    }
    if(bigint_add_mod(G, D, C, p) != 0) { // G = D + C
        goto cleanup;
    }
    if(bigint_add_mod(H, B, A, p) != 0) { // H = B + A
        goto cleanup;
    }

    // Final Coordinates
    if(bigint_mul_mod(res->X, E, F, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(res->Y, G, H, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(res->Z, F, G, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(res->T, E, H, p) != 0) {
        goto cleanup;
    }

    err = 0;
    // Cleanup (Omitted for brevity, but essential in your lib)
cleanup:
    bigint_destroy(YpX1); bigint_destroy(YmX1); bigint_destroy(YpX2); bigint_destroy(YmX2);
    bigint_destroy(A); bigint_destroy(B); bigint_destroy(C); bigint_destroy(D);
    bigint_destroy(E); bigint_destroy(F); bigint_destroy(G); bigint_destroy(H);
    bigint_destroy(d2);
    return err;
}

static int8_t ed25519_point_double(ed25519_point_t* R, const ed25519_point_t* P, const bigint_t* p) {
    if (!R || !P || !p) {
        return -1;
    }

    int8_t err = -1;

    bigint_t * A = bigint_create();
    bigint_t * B = bigint_create();
    bigint_t * C = bigint_create();
    bigint_t * D = bigint_create();
    bigint_t * E = bigint_create();
    bigint_t * F = bigint_create();
    bigint_t * G = bigint_create();
    bigint_t * H = bigint_create();

    if (!A || !B || !C || !D || !E || !F || !G || !H) {
        goto cleanup;
    }

    // Step 1: A = X^2
    if (bigint_mul_mod(A, P->X, P->X, p) != 0) {
        goto cleanup;
    }

    // Step 2: B = Y^2
    if (bigint_mul_mod(B, P->Y, P->Y, p) != 0) {
        goto cleanup;
    }

    // Step 3: C = 2 * Z^2
    if (bigint_mul_mod(C, P->Z, P->Z, p) != 0) {
        goto cleanup;
    }
    if (bigint_add_mod(C, C, C, p) != 0) {
        goto cleanup;
    }

    // Step 4: D = -A  (a = -1)
    if (bigint_sub_mod(D, p, A, p) != 0) {
        goto cleanup;
    }

    // Step 5: E = 2 * X * Y
    if(bigint_mul_mod(E, P->X, P->Y, p) != 0) {
        goto cleanup;
    }
    if(bigint_add_mod(E, E, E, p) != 0) {
        goto cleanup;
    }

    // Step 6: G = D + B
    if (bigint_add_mod(G, D, B, p) != 0) {
        goto cleanup;
    }

    // Step 7: F = G - C
    if (bigint_sub_mod(F, G, C, p) != 0) {
        goto cleanup;
    }

    // Step 8: H = D - B
    if (bigint_sub_mod(H, D, B, p) != 0) {
        goto cleanup;
    }

    // Step 9: Compute new coordinates
    if (bigint_mul_mod(R->X, E, F, p) != 0) {
        goto cleanup;
    }
    if (bigint_mul_mod(R->Y, G, H, p) != 0) {
        goto cleanup;
    }
    if (bigint_mul_mod(R->Z, F, G, p) != 0) {
        goto cleanup;
    }
    if (bigint_mul_mod(R->T, E, H, p) != 0) {
        goto cleanup;
    }

    err = 0;

cleanup:
    bigint_destroy(A); bigint_destroy(B); bigint_destroy(C);
    bigint_destroy(D); bigint_destroy(E); bigint_destroy(F);
    bigint_destroy(G); bigint_destroy(H);

    return err;
}

#if 0
int8_t test_ed25519_point_add();
int8_t test_ed25519_point_add() {
    // This function would contain test cases to validate ed25519_point_add
    int8_t err = -1;

    bigint_t * lhs = bigint_create();
    bigint_t * rhs = bigint_create();
    ed25519_point_t P, Q, R;
    bigint_t* one = bigint_one();
    bigint_t * expected_2g_y = bigint_create();
    bigint_t * p = bigint_create();
    if(bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto cleanup;
    }
    P.X = bigint_create(); P.Y = bigint_create(); P.Z = bigint_create(); P.T = bigint_create();
    Q.X = bigint_create(); Q.Y = bigint_create(); Q.Z = bigint_create(); Q.T = bigint_create();
    R.X = bigint_create(); R.Y = bigint_create(); R.Z = bigint_create(); R.T = bigint_create();
    // Initialize P and Q with known values
    // Identity Point (0, 1, 1, 0)
    if(bigint_set_zero(P.X) != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(P.Y, 1) != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(P.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_set_zero(P.T) != 0) {
        goto cleanup;
    }

    // Base Point G
    if(bigint_set_str(Q.X, "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0) {
        goto cleanup;
    }
    if(bigint_set_str(Q.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(Q.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(Q.T, Q.X, Q.Y, p) != 0) {
        goto cleanup;
    }
    // Verify R against expected results
    if(ed25519_point_add(&R, &P, &Q, p) != 0) {
        goto cleanup;
    }

    // R should equal Q since P is the identity point
    // Intermediate variables for cross-multiplication

    // 1. Verify X ratio: R.X * Q.Z == Q.X * R.Z
    if(bigint_mul_mod(lhs, R.X, Q.Z, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, Q.X, R.Z, p) != 0) {
        goto cleanup;
    }
    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "X coordinate ratio mismatch!");
    }

    // 2. Verify Y ratio: R.Y * Q.Z == Q.Y * R.Z
    if(bigint_mul_mod(lhs, R.Y, Q.Z, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, Q.Y, R.Z, p) != 0) {
        goto cleanup;
    }
    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Y coordinate ratio mismatch!");
    }

    // 3. Verify T (Auxiliary) ratio: R.T * Q.Z == (Q.X * Q.Y) * R.Z
    // Note: Since Q.Z is 1, it's just R.T == (Q.X * Q.Y) * R.Z
    // But R.T must also satisfy R.X * R.Y == R.Z * R.T
    if(bigint_mul_mod(lhs, R.X, R.Y, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, R.Z, R.T, p) != 0) {
        goto cleanup;
    }
    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "I + G = G test failed on T coordinate!");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "I + G = G test passed!");
    }

    // --- TEST CASE: G + G = 2G ---
    // Copy Q (which is G) into P
    if(bigint_set_bigint(P.X, Q.X) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(P.Y, Q.Y) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(P.Z, Q.Z) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(P.T, Q.T) != 0) {
        goto cleanup;
    }

    if(ed25519_point_add(&R, &P, &Q, p) != 0) {
        goto cleanup;
    }

    // Expected Affine Y for 2G
    if(bigint_set_str(expected_2g_y, "2260cdf3092329c21da25ee8c9a21f5697390f51643851560e5f46ae6af8a3c9") != 0) {
        goto cleanup;
    }

    // Ratio Check: R.Y * 1 == expected_2g_y * R.Z
    if(bigint_mul_mod(lhs, R.Y, one, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, expected_2g_y, R.Z, p) != 0) {
        goto cleanup;
    }

    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "G + G = 2G test failed!");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "G + G = 2G test passed!");
    }

    // --- TEST CASE: G + 2G = 3G ---
    // P is now G, Q is now R (which is 2G)
    if(bigint_set_bigint(Q.X, R.X) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(Q.Y, R.Y) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(Q.Z, R.Z) != 0) {
        goto cleanup;
    }
    if(bigint_set_bigint(Q.T, R.T) != 0) {
        goto cleanup;
    }
    // Reset P to G
    if(bigint_set_str(P.X, "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0) {
        goto cleanup;
    }
    if(bigint_set_str(P.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(P.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(P.T, P.X, P.Y, p) != 0) {
        goto cleanup;
    }

    if(ed25519_point_add(&R, &P, &Q, p) != 0) {
        goto cleanup;
    }

    // Expected Affine Y for 3G
    if(bigint_set_str(expected_2g_y, "1267b1d177ee69aba126a18e60269ef79f16ec176724030402c3684878f5b4d4") != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(lhs, R.Y, one, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, expected_2g_y, R.Z, p) != 0) {
        goto cleanup;
    }

    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "G + 2G = 3G test failed!");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "G + 2G = 3G test passed!");
    }

    // Base Point G
    if(bigint_set_str(P.X, "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0) {
        goto cleanup;
    }
    if(bigint_set_str(P.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(P.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(P.T, P.X, P.Y, p) != 0) {
        goto cleanup;
    }

    // Base Point G
    if(bigint_sub_mod(Q.X, p, P.X, p) != 0) {
        goto cleanup;
    }
    if(bigint_set_str(Q.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(Q.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(Q.T, Q.X, Q.Y, p) != 0) {
        goto cleanup;
    }
    // Verify R against expected results
    if(ed25519_point_add(&R, &P, &Q, p) != 0) {
        goto cleanup;
    }

    // R should equal the identity point since Q is -G
    // Ratio Check: R.Y * 1 == 1 * R.Z
    if(bigint_mul_mod(lhs, R.Y, one, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(rhs, one, R.Z, p) != 0) {
        goto cleanup;
    }
    if (bigint_cmp(lhs, rhs) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "G + (-G) = I test failed!");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "G + (-G) = I test passed!");
    }

    err = 0;
cleanup:
    bigint_destroy(p);
    bigint_destroy(P.X); bigint_destroy(P.Y); bigint_destroy(P.Z); bigint_destroy(P.T);
    bigint_destroy(Q.X); bigint_destroy(Q.Y); bigint_destroy(Q.Z); bigint_destroy(Q.T);
    bigint_destroy(R.X); bigint_destroy(R.Y); bigint_destroy(R.Z); bigint_destroy(R.T);
    bigint_destroy(lhs); bigint_destroy(rhs);
    bigint_destroy(one);
    bigint_destroy(expected_2g_y);
    return err;
}


int8_t test_ed25519_point_double();
int8_t test_ed25519_point_double() {
    int8_t err = -1;
    bigint_t* p = NULL;
    bigint_t * invZ1 = bigint_create();
    bigint_t * invZ2 = bigint_create();
    bigint_t * y1 = bigint_create();
    bigint_t * y2 = bigint_create();
    bigint_t* one = bigint_create();
    char_t* str = NULL;

    if(bigint_set_uint64(one, 1) != 0) {
        return -1;
    }

    ed25519_point_t G, R, R_add;
    p = bigint_create();
    if(bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto cleanup;
    }

    G.X = bigint_create(); G.Y = bigint_create(); G.Z = bigint_create(); G.T = bigint_create();
    R.X = bigint_create(); R.Y = bigint_create(); R.Z = bigint_create(); R.T = bigint_create();
    R_add.X = bigint_create(); R_add.Y = bigint_create(); R_add.Z = bigint_create(); R_add.T = bigint_create();

    // 1. Initialize G
    if(bigint_set_str(G.X, "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0) {
        goto cleanup;
    }
    if(bigint_set_str(G.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }
    if(bigint_set_uint64(G.Z, 1) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(G.T, G.X, G.Y, p) != 0) {
        goto cleanup;
    }

    str = bigint_to_str(G.X);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Base Point G X Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(G.Y);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Base Point G Y Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(G.Z);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Base Point G Z Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(G.T);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Base Point G T Coordinate: %s", str);
    memory_free(str);

    // 2. Perform Double
    ed25519_point_double(&R, &G, p);

    // 3. Expected Affine Y for 2G (Big Endian)
    // 733D031C813D6D2CD6713D74F7CC4420B3A635D9D2F9508326C9A3F86AED4484
    str = bigint_to_str(R.X);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Doubled X Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R.Y);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Doubled Y Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R.Z);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Doubled Z Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R.T);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Doubled T Coordinate: %s", str);
    memory_free(str);

    // Perform the Ratio Check: R.Y * 1 == Expected_Y * R.Z
    if(ed25519_point_add(&R_add, &G, &G, p) != 0) { // R_add = G + G (to verify addition matches doubling)
        goto cleanup;
    }

    str = bigint_to_str(R_add.X);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Added X Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R_add.Y);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Added Y Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R_add.Z);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Added Z Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(R_add.T);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Added T Coordinate: %s", str);
    memory_free(str);

    // y = Y * Z^(p-2) mod p
    if(bigint_mod_inv(invZ1, R.Z, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(y1, R.Y, invZ1, p) != 0) {
        goto cleanup;
    }

    if(bigint_mod_inv(invZ2, R_add.Z, p) != 0) {
        goto cleanup;
    }
    if(bigint_mul_mod(y2, R_add.Y, invZ2, p) != 0) {
        goto cleanup;
    }

    str = bigint_to_str(y1);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Doubled Affine Y Coordinate: %s", str);
    memory_free(str);
    str = bigint_to_str(y2);
    PRINTLOG(CRYPTOLIB, LOG_INFO, "Added Affine Y Coordinate: %s", str);
    memory_free(str);

    if (bigint_cmp(y1, y2) == 0) {
        PRINTLOG(CRYPTOLIB, LOG_INFO, "ed25519_point_double test passed");
        err = 0;
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "ed25519_point_double test failed!");
    }

    // Cleanup...
cleanup:
    bigint_destroy(p);
    bigint_destroy(G.X); bigint_destroy(G.Y); bigint_destroy(G.Z); bigint_destroy(G.T);
    bigint_destroy(R.X); bigint_destroy(R.Y); bigint_destroy(R.Z); bigint_destroy(R.T);
    bigint_destroy(R_add.X); bigint_destroy(R_add.Y); bigint_destroy(R_add.Z); bigint_destroy(R_add.T);
    bigint_destroy(one);
    bigint_destroy(invZ1);
    bigint_destroy(invZ2);
    bigint_destroy(y1);
    bigint_destroy(y2);
    return err;
}
#endif

static int8_t ed25519_scalar_mult(uint8_t out_pub[32], const uint8_t scalar[32]) {
    if (!out_pub || !scalar) {
        return -1;
    }

    int8_t err = -1;

    bigint_t * p = NULL, * invZ = NULL, * x_aff = NULL, * y_aff = NULL;
    ed25519_point_t * R0 = NULL, * R1 = NULL;

    /* Field prime p = 2^255 - 19 */
    p = bigint_create();
    if (bigint_set_str(p,
                       "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto cleanup;
    }

    /* Allocate points */
    R0 = memory_malloc(sizeof(ed25519_point_t));
    R1 = memory_malloc(sizeof(ed25519_point_t));
    if (!R0 || !R1) {
        goto cleanup;
    }

    /* R0 = identity (0,1,1,0) */
    R0->X = bigint_zero();
    R0->Y = bigint_one();
    R0->Z = bigint_one();
    R0->T = bigint_zero();

    /* R1 = base point */
    R1->X = bigint_create();
    R1->Y = bigint_create();
    R1->Z = bigint_one();
    R1->T = bigint_create();

    if (bigint_set_str(R1->X,
                       "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0 ||
        bigint_set_str(R1->Y,
                       "6666666666666666666666666666666666666666666666666666666666666658") != 0) {
        goto cleanup;
    }

    if (bigint_mul_mod(R1->T, R1->X, R1->Y, p) != 0) {
        goto cleanup;
    }

    /* Edwards ladder */
    uint8_t swap = 0;
    for (int i = 255; i >= 0; i--) {
        uint8_t bit = (scalar[i >> 3] >> (i & 7)) & 1;
        uint8_t s = swap ^ bit;

        if(bigint_cswap(R0->X, R1->X, s) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->Y, R1->Y, s) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->Z, R1->Z, s) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->T, R1->T, s) != 0) {
            goto cleanup;
        }

        swap = bit;

        if(ed25519_point_add(R1, R0, R1, p) != 0) {
            goto cleanup;
        }

        if(ed25519_point_double(R0, R0, p) != 0) {
            goto cleanup;
        }
    }
    // Final swap
    if(bigint_cswap(R0->X, R1->X, swap) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->Y, R1->Y, swap) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->Z, R1->Z, swap) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->T, R1->T, swap) != 0) {
        goto cleanup;
    }

    /* Convert R0 to affine */
    invZ  = bigint_create();
    x_aff = bigint_create();
    y_aff = bigint_create();
    if (!invZ || !x_aff || !y_aff) {
        goto cleanup;
    }

    if (bigint_mod_inv(invZ, R0->Z, p) != 0) {
        goto cleanup;
    }
    if (bigint_mul_mod(x_aff, R0->X, invZ, p) != 0) {
        goto cleanup;
    }
    if (bigint_mul_mod(y_aff, R0->Y, invZ, p) != 0) {
        goto cleanup;
    }

    /* Encode compressed public key */
    if (bigint_to_bytes_le(y_aff, out_pub, 32) != 0) {
        goto cleanup;
    }
    if (bigint_is_odd(x_aff)) {
        out_pub[31] |= 0x80;
    }

    err = 0;

cleanup:
    bigint_destroy(x_aff);
    bigint_destroy(y_aff);
    bigint_destroy(invZ);
    bigint_destroy(p);

    if (R0) {
        bigint_destroy(R0->X);
        bigint_destroy(R0->Y);
        bigint_destroy(R0->Z);
        bigint_destroy(R0->T);
        memory_free(R0);
    }

    if (R1) {
        bigint_destroy(R1->X);
        bigint_destroy(R1->Y);
        bigint_destroy(R1->Z);
        bigint_destroy(R1->T);
        memory_free(R1);
    }

    return err;
}

int8_t ed25519_derive_pubkey(uint8_t pub_out[ED25519_PUBLIC_KEY_RAW_LEN], const uint8_t priv_seed[ED25519_PRIVATE_KEY_RAW_LEN]) {
    if (!pub_out || !priv_seed) {
        return -1;
    }

    uint8_t* az = sha512_hash(priv_seed, ED25519_PRIVATE_KEY_RAW_LEN); // Step 2: Hash
    if (!az) {
        return -1;
    }

    uint8_t a[32];
    memory_memcopy(az, a, 32);
    memory_free(az);

    // Step 3 & 4: Scalar Extraction & Clamping
    a[0]  &= 248;
    a[31] &= 127;
    a[31] |= 64;

    // Step 5 & 6: Multiply (Edwards) and Encode
    // IMPORTANT: Use the scalar_mult without clamping inside it!
    int8_t res = ed25519_scalar_mult(pub_out, a);

    return res;
}

static int8_t ed25519_reduce_L(uint8_t out[32], const uint8_t in_64[64]) {
    bigint_t* val = bigint_create();
    bigint_t* L = bigint_create();

    // Set L
    if(bigint_set_str(L, "1000000000000000000000000000000014DEF9DEA2F79CD65812631A5CF5D3ED") != 0) {
        bigint_destroy(val);
        bigint_destroy(L);
        return -1;
    }

    // Load the 64-byte hash (Little Endian)
    if(bigint_from_bytes_le(val, in_64, 64) != 0) {
        bigint_destroy(val);
        bigint_destroy(L);
        return -1;
    }

    // Reduce: val = val % L
    if(bigint_mod(val, val, L) != 0) {
        bigint_destroy(val);
        bigint_destroy(L);
        return -1;
    }

    // Export result
    if(bigint_to_bytes_le(val, out, 32) != 0) {
        bigint_destroy(val);
        bigint_destroy(L);
        return -1;
    }

    bigint_destroy(val);
    bigint_destroy(L);
    return 0;
}

int8_t ed25519_sign(uint8_t sig[ED25519_SIGNATURE_LEN], const uint8_t* msg, size_t msg_len,
                    const uint8_t priv_seed[ED25519_PRIVATE_KEY_RAW_LEN]) {
    uint8_t pub_key[32];
    if (ed25519_derive_pubkey(pub_key, priv_seed) != 0) {
        return -1;
    }

    uint8_t r_reduced[32];
    uint8_t k_reduced[32];
    uint8_t* az = NULL;
    uint8_t* nonce_hash = NULL;
    uint8_t* k_hash = NULL;

    int8_t err = -1;

    az = sha512_hash(priv_seed, 32);
    if (!az) {
        goto cleanup;
    }

    /* Clamp the left half (a) to create the secret scalar */
    az[0]  &= 248;
    az[31] &= 63;
    az[31] |= 64;

    sha512_ctx_t* ctx;

    /* 2. Generate deterministic nonce 'r' = SHA512(prefix + msg) */
    ctx = sha512_init();
    sha512_update(ctx, az + 32, 32); // Use the right half of the hash
    sha512_update(ctx, msg, msg_len);
    nonce_hash = sha512_final(ctx);

    if (!nonce_hash) {
        goto cleanup;
    }

    /* 3. Reduce r modulo L */
    if(ed25519_reduce_L(r_reduced, nonce_hash) != 0) {
        goto cleanup;
    }

    /* 4. Calculate R = r * G */
    /* Note: We use our scalar_mult, but r_reduced is already clamped by math */
    if (ed25519_scalar_mult(sig, r_reduced) != 0) {
        return -1;
    }

    /* 5. Calculate k = SHA512(encoded R + Public Key + msg) */
    ctx = sha512_init();
    sha512_update(ctx, sig, 32); // R is stored in the first half of sig
    sha512_update(ctx, pub_key, 32);
    sha512_update(ctx, msg, msg_len);
    k_hash = sha512_final(ctx);

    if (!k_hash) {
        goto cleanup;
    }

    /* Reduce k modulo L */
    if(ed25519_reduce_L(k_reduced, k_hash) != 0) {
        goto cleanup;
    }

    /* 6. Calculate S = (r + k * a) mod L */
    bigint_t * br = bigint_create(), * bk = bigint_create();
    bigint_t * ba = bigint_create(), * bL = bigint_create();
    bigint_t * bs = bigint_create();

    if (!br || !bk || !ba || !bL || !bs) {
        goto cleanup2;
    }

    if(bigint_from_bytes_le(br, r_reduced, 32) != 0 ||
       bigint_from_bytes_le(bk, k_reduced, 32) != 0 ||
       bigint_from_bytes_le(ba, az, 32) != 0 ||
       bigint_set_str(bL, "1000000000000000000000000000000014DEF9DEA2F79CD65812631A5CF5D3ED") != 0) {
        goto cleanup2;
    }

    // S = (k * a)
    if(bigint_mul(bs, bk, ba) != 0) {
        goto cleanup2;
    }
    // S = (r + (k * a))
    if(bigint_add(bs, bs, br) != 0) {
        goto cleanup2;
    }
    // S = S % L
    if(bigint_mod(bs, bs, bL) != 0) {
        goto cleanup2;
    }

    /* Export S to the second half of the signature */
    if(bigint_to_bytes_le(bs, sig + 32, 32) != 0) {
        goto cleanup2;
    }

    err = 0;

    /* 7. Cleanup */
cleanup2:
    bigint_destroy(br); bigint_destroy(bk);
    bigint_destroy(ba); bigint_destroy(bL);
    bigint_destroy(bs);

cleanup:
    memory_free(az);
    memory_free(nonce_hash);
    memory_free(k_hash);

    return err;
}

static int8_t ed25519_decode_point(ed25519_point_t* P, const uint8_t encoded[32]) {
    if (!P || !encoded) {
        return -1;
    }

    int8_t err = -1;
    // P->X, P->Y, P->T are pointers; ensure they are handled safely.
    // We create locals for math, then move them to P on success.
    bigint_t * p = bigint_create(), * y = bigint_create(), * x = bigint_create();
    bigint_t * y_squared = bigint_create(), * u = bigint_create(), * v = bigint_create();
    bigint_t * v_inv = bigint_create(), * x_squared = bigint_create();
    bigint_t * ED25519_D = bigint_create(), * one = bigint_one();

    if (!p || !y || !x || !y_squared || !u || !v || !v_inv || !x_squared || !ED25519_D || !one) {
        goto fail;
    }

    if(bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set prime p for Ed25519.");
        goto fail;
    }
    if(bigint_set_str(ED25519_D, "52036CEE2B6FFE738CC740797779E89800700A4D4141D8AB75EB4DCA135978A3") != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set curve parameter d for Ed25519.");
        goto fail;
    }

    if (bigint_from_bytes_le(y, encoded, 32) != 0) {
        goto fail;
    }

    uint8_t x_bit = (encoded[31] >> 7) & 1;
    if(bigint_clear_bit(y, 255) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to clear x bit from y coordinate.");
        goto fail;
    }

    // x^2 = (y^2 - 1) / (d * y^2 + 1) mod p
    if(bigint_mul_mod(y_squared, y, y, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute y^2 mod p.");
        goto fail;
    }
    if(bigint_sub_mod(u, y_squared, one, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute u = y^2 - 1 mod p.");
        goto fail;
    }
    if(bigint_mul_mod(v, ED25519_D, y_squared, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute v = d * y^2 mod p.");
        goto fail;
    }
    if(bigint_add_mod(v, v, one, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute v = d * y^2 + 1 mod p.");
        goto fail;
    }

    if (bigint_mod_inv(v_inv, v, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute modular inverse of v.");
        goto fail;
    }

    if(bigint_mul_mod(x_squared, u, v_inv, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute x^2 coordinate.");
        goto fail;
    }

    if (bigint_mod_sqrt(x, x_squared, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute modular square root for x coordinate.");
        goto fail;
    }

    // Check parity and adjust
    // If x=0 and x_bit=1, this is a forbidden point (small subgroup check)
    if (bigint_is_zero(x) && x_bit == 1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decoded point has x=0 but odd bit set, invalid point.");
        goto fail;
    }

    if (bigint_is_odd(x) != x_bit) {
        if(bigint_sub_mod(x, p, x, p) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to adjust x coordinate parity.");
            goto fail;
        }
    }

    // Success! Move ownership to P or clone them.
    // If P already has allocated bigints, destroy them first or use bigint_set.
    if(bigint_set_bigint(P->X, x) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set X coordinate of point.");
        goto fail;
    }
    if(bigint_set_bigint(P->Y, y) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set Y coordinate of point.");
        goto fail;
    }
    if(bigint_set_uint64(P->Z, 1) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set Z coordinate of point.");
        goto fail;
    }
    if(bigint_mul_mod(P->T, P->X, P->Y, p) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute T coordinate of point.");
        goto fail;
    }

    err = 0;
fail:
    bigint_destroy(p); bigint_destroy(y); bigint_destroy(x);
    bigint_destroy(y_squared); bigint_destroy(u); bigint_destroy(v);
    bigint_destroy(v_inv); bigint_destroy(x_squared);
    bigint_destroy(ED25519_D); bigint_destroy(one);
    return err;
}

static int8_t ed25519_points_equal(const ed25519_point_t* p1, const ed25519_point_t* p2) {
    // Check if X1 * Z2 == X2 * Z1 and Y1 * Z2 == Y2 * Z1
    bigint_t* left_X = bigint_create();
    bigint_t* right_X = bigint_create();
    bigint_t* left_Y = bigint_create();
    bigint_t* right_Y = bigint_create();
    bigint_t* p = bigint_create();

    if (!left_X || !right_X || !left_Y || !right_Y || !p) {
        bigint_destroy(left_X); bigint_destroy(right_X);
        bigint_destroy(left_Y); bigint_destroy(right_Y);
        bigint_destroy(p);
        return -1;
    }

    if (bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto fail;
    }

    // left_X = X1 * Z2
    if (bigint_mul_mod(left_X, p1->X, p2->Z, p) != 0) {
        goto fail;
    }
    // right_X = X2 * Z1
    if (bigint_mul_mod(right_X, p2->X, p1->Z, p) != 0) {
        goto fail;
    }
    // left_Y = Y1 * Z2
    if (bigint_mul_mod(left_Y, p1->Y, p2->Z, p) != 0) {
        goto fail;
    }
    // right_Y = Y2 * Z1
    if (bigint_mul_mod(right_Y, p2->Y, p1->Z, p) != 0) {
        goto fail;
    }

    int8_t res = (bigint_cmp(left_X, right_X) == 0) && (bigint_cmp(left_Y, right_Y) == 0) ? 1 : 0;

    bigint_destroy(left_X); bigint_destroy(right_X);
    bigint_destroy(left_Y); bigint_destroy(right_Y);
    bigint_destroy(p);
    return res;
fail:
    bigint_destroy(left_X); bigint_destroy(right_X);
    bigint_destroy(left_Y); bigint_destroy(right_Y);
    bigint_destroy(p);
    return -1;
}

static int8_t ed25519_scalar_mult_generic(ed25519_point_t* R, const uint8_t scalar[32], const ed25519_point_t* P) {
    if (!R || !scalar) {
        return -1;
    }

    int8_t err = -1;
    bigint_t * p = NULL, * inv_Z = NULL;
    ed25519_point_t * R0 = NULL, * R1 = NULL;

    p = bigint_create();
    if (bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        goto cleanup;
    }

    R0 = (ed25519_point_t*)memory_malloc(sizeof(ed25519_point_t));
    R1 = (ed25519_point_t*)memory_malloc(sizeof(ed25519_point_t));
    if (!R0 || !R1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to allocate memory for points R0 and R1.");
        goto cleanup;
    }

    /* 2. Initialize R0 to Identity Point: (0, 1, 1, 0) */
    R0->X = bigint_zero();
    R0->Y = bigint_one();
    R0->Z = bigint_one();
    R0->T = bigint_zero();

    /* 3. Initialize R1 to Point P */
    if (P) {
        R1->X = bigint_create();
        R1->Y = bigint_create();
        R1->Z = bigint_create();
        R1->T = bigint_create();

        if (bigint_set_bigint(R1->X, P->X) != 0 ||
            bigint_set_bigint(R1->Y, P->Y) != 0 ||
            bigint_set_bigint(R1->Z, P->Z) != 0 ||
            bigint_set_bigint(R1->T, P->T) != 0) {
            goto cleanup;
        }
        PRINTLOG(CRYPTOLIB, LOG_DEBUG, "Initialized R1 to provided point P for scalar multiplication.");
    } else {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Point P must be provided for generic scalar multiplication.");
        goto cleanup; // P must be provided for generic scalar mult
    }

    /* 4. Constant-Time Montgomery Ladder (Edwards Variant) */
    uint8_t dummy_bit = 0;
    for (int64_t i = 255; i >= 0; i--) {
        uint8_t bit = (scalar[i >> 3] >> (i & 7)) & 1;
        uint8_t swap = bit ^ dummy_bit;

        /* Swap R0 and R1 if the bit changed */
        if(bigint_cswap(R0->X, R1->X, swap) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->Y, R1->Y, swap) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->Z, R1->Z, swap) != 0) {
            goto cleanup;
        }
        if(bigint_cswap(R0->T, R1->T, swap) != 0) {
            goto cleanup;
        }
        dummy_bit = bit;
        /* Core Math: R1 = R0 + R1, R0 = 2 * R0 */
        if (ed25519_point_add(R1, R0, R1, p) != 0) {
            goto cleanup;
        }
        if (ed25519_point_double(R0, R0, p) != 0) {
            goto cleanup;
        }
    }
    /* Final swap to ensure result is in R0 */
    if(bigint_cswap(R0->X, R1->X, dummy_bit) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->Y, R1->Y, dummy_bit) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->Z, R1->Z, dummy_bit) != 0) {
        goto cleanup;
    }
    if(bigint_cswap(R0->T, R1->T, dummy_bit) != 0) {
        goto cleanup;
    }
    /* Move result to R */
    if (bigint_set_bigint(R->X, R0->X) != 0 ||
        bigint_set_bigint(R->Y, R0->Y) != 0 ||
        bigint_set_bigint(R->Z, R0->Z) != 0 ||
        bigint_set_bigint(R->T, R0->T) != 0) {
        goto cleanup;
    }
    err = 0;
cleanup:
    bigint_destroy(p);
    if (inv_Z) {
        bigint_destroy(inv_Z);
    }
    if (R0) {
        bigint_destroy(R0->X); bigint_destroy(R0->Y);
        bigint_destroy(R0->Z); bigint_destroy(R0->T);
        memory_free(R0);
    }
    if (R1) {
        bigint_destroy(R1->X); bigint_destroy(R1->Y);
        bigint_destroy(R1->Z); bigint_destroy(R1->T);
        memory_free(R1);
    }
    return err;
}

int8_t ed25519_verify(const uint8_t sig[ED25519_SIGNATURE_LEN],
                      const uint8_t* msg, size_t msg_len,
                      const uint8_t pub_key[ED25519_PUBLIC_KEY_RAW_LEN]) {
    uint8_t* k_hash = NULL;
    uint8_t k_reduced[32];
    sha512_ctx_t* ctx;

    int8_t err = -1;

    ed25519_point_t R = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };
    ed25519_point_t A = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };
    ed25519_point_t p_left = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };
    ed25519_point_t p_kA = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };
    ed25519_point_t p_right = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };
    ed25519_point_t G = {
        .X = bigint_create(),
        .Y = bigint_create(),
        .Z = bigint_create(),
        .T = bigint_create()
    };

    bigint_t* p_field = NULL;

    p_field = bigint_create();
    if (bigint_set_str(p_field, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED") != 0) {
        bigint_destroy(p_field);
        goto fail;
    }

    if(bigint_set_str(G.X, "216936D3CD6E53FEC0A4E231FDD6DC5C692CC7609525A7B2C9562D608F25D51A") != 0 ||
       bigint_set_str(G.Y, "6666666666666666666666666666666666666666666666666666666666666658") != 0 ||
       bigint_set_uint64(G.Z, 1) != 0 ||
       bigint_mul_mod(G.T, G.X, G.Y, p_field) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to set base point G.");
        goto fail;
    }

    // 1. Decode R (first 32 bytes of signature) and A (public key)
    if (ed25519_decode_point(&R, sig) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode R point.");
        goto fail;
    }
    if (ed25519_decode_point(&A, pub_key) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode A point.");
        goto fail;
    }

    // 2. Calculate k = SHA512(R || A || msg) mod L
    ctx = sha512_init();
    sha512_update(ctx, sig, 32); // Raw R
    sha512_update(ctx, pub_key, 32); // Raw A
    sha512_update(ctx, msg, msg_len);
    k_hash = sha512_final(ctx);

    if (!k_hash) {
        goto fail;
    }

    if(ed25519_reduce_L(k_reduced, k_hash) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to reduce k modulo L.");
        goto fail;
    }

    // 3. Compute P_left = [S]G
    // S is the second 32 bytes of the signature.
    // No clamping here! Use raw S.
    if (ed25519_scalar_mult_generic(&p_left, sig + 32, &G) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute [S]G.");
        goto fail;
    }

    // 4. Compute P_right = R + [k]A
    if (ed25519_scalar_mult_generic(&p_kA, k_reduced, &A) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute [k]A.");
        goto fail;
    }

    if (ed25519_point_add(&p_right, &R, &p_kA, p_field) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to compute R + [k]A.");
        goto fail;
    }

    // 5. Comparison (Constant time check)
    // Points are equal if (X1*Z2 == X2*Z1) and (Y1*Z2 == Y2*Z1)
    int8_t res = ed25519_points_equal(&p_left, &p_right);

    err = (res == 1) ? 0 : -1; // 0 = valid, -1 = invalid

fail:
    // ... Cleanup ...
    bigint_destroy(R.X); bigint_destroy(R.Y); bigint_destroy(R.Z); bigint_destroy(R.T);
    bigint_destroy(A.X); bigint_destroy(A.Y); bigint_destroy(A.Z); bigint_destroy(A.T);
    bigint_destroy(p_left.X); bigint_destroy(p_left.Y); bigint_destroy(p_left.Z); bigint_destroy(p_left.T);
    bigint_destroy(p_kA.X); bigint_destroy(p_kA.Y); bigint_destroy(p_kA.Z); bigint_destroy(p_kA.T);
    bigint_destroy(p_right.X); bigint_destroy(p_right.Y); bigint_destroy(p_right.Z); bigint_destroy(p_right.T);
    if (p_field) {
        bigint_destroy(p_field);
    }
    bigint_destroy(G.X); bigint_destroy(G.Y); bigint_destroy(G.Z); bigint_destroy(G.T);
    memory_free(k_hash);
    return err;
}

int8_t ed25519_generate_keypair(uint8_t out_priv[ED25519_PRIVATE_KEY_RAW_LEN], uint8_t out_pub[ED25519_PUBLIC_KEY_RAW_LEN]) {
    if (!out_priv || !out_pub) {
        return -1;
    }

    // 1. Generate 32 random bytes for the private seed
    get_random_bytes(out_priv, ED25519_PRIVATE_KEY_RAW_LEN);

    // 2. Derive the public key from the private seed
    if (ed25519_derive_pubkey(out_pub, out_priv) != 0) {
        return -1;
    }

    return 0;
}

static int8_t _pem_x_ed_25519_read_key(const char_t*           pem,
                                       der_object_identifier_t expected_oid,
                                       boolean_t               is_pub_key,
                                       size_t                  expected_key_len,
                                       uint8_t*                out_key) {
    if (!pem || !out_key) {
        return -1;
    }

    uint8_t* decoded_data = NULL;
    size_t decoded_len = 0;
    const char_t* pem_type = is_pub_key ? "PUBLIC KEY" : "PRIVATE KEY";

    // 1. Decode PEM (Base64 -> Binary DER)
    if (pem_decode(pem_type, pem, strlen(pem), &decoded_data, &decoded_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to decode PEM");
        return -1;
    }

    // 2. Init Decoder
    der_decoder_t* der = der_decoder_new(decoded_data, decoded_len);
    if (!der) {
        memory_free(decoded_data);
        return -1;
    }

    // --- PARSE ROOT ---
    // Outer Sequence (SubjectPublicKeyInfo or PrivateKeyInfo)
    if (der_decoder_start_sequence(der) != 0) {
        goto error;
    }

    // --- PARSE VERSION (Private Key Only) ---
    if (!is_pub_key) {
        int64_t version;
        if (der_decoder_decode_integer(der, &version) != 0 || version != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid PKCS#8 Version");
            goto error;
        }
    }

    // --- PARSE ALGORITHM IDENTIFIER ---
    // This is a SEQUENCE containing an OID
    if (der_decoder_start_sequence(der) != 0) {
        goto error;
    }

    der_object_identifier_t oid;
    if (der_decoder_decode_object_identifier(der, &oid) != 0) {
        goto error;
    }

    if (oid != expected_oid) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "OID Mismatch");
        goto error;
    }

    // FIX 1: Explicitly end/exit the AlgorithmIdentifier Sequence.
    // This skips any optional parameters (should be none for Ed25519) and aligns cursor.
    if (der_decoder_end_sequence(der) != 0) {
        goto error;
    }

    // --- PARSE KEY DATA ---
    uint8_t* raw_der_key = NULL;
    size_t raw_der_len = 0;

    if (is_pub_key) {
        // Public Key: BIT STRING containing the key
        if (der_decoder_decode_bit_string(der, &raw_der_key, &raw_der_len) != 0) {
            goto error;
        }
    } else {
        // Private Key: OCTET STRING (Wrapper) -> OCTET STRING (CurvePrivateKey)
        // 1. Enter the wrapper field
        if (der_decoder_start_octet_string(der) != 0) {
            goto error;
        }

        // 2. Read the inner CurvePrivateKey
        if (der_decoder_decode_octet_string(der, &raw_der_key, &raw_der_len) != 0) {
            goto error;
        }

        // 3. Exit the wrapper (Optional depending on your decoder, but good practice)
        der_decoder_end_octet_string(der);
    }

    // --- VALIDATE & COPY ---
    if (raw_der_len != expected_key_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Key length mismatch. Got %llu, expected %llu", raw_der_len, expected_key_len);
        memory_free(raw_der_key);
        goto error;
    }

    // FIX 2: Check if we are at the end of the OUTER sequence
    if (der_decoder_end_sequence(der) != 0) {
        // If we can't exit the root sequence, structure is malformed
        memory_free(raw_der_key);
        goto error;
    }

    // Copy to output
    memory_memcopy(raw_der_key, out_key, expected_key_len);

    // Cleanup
    memory_free(raw_der_key);
    der_decoder_destroy(der);
    memory_free(decoded_data);
    return 0;

error:
    der_decoder_destroy(der);
    memory_free(decoded_data);
    return -1;
}

int8_t pem_read_x25519_private_key(const char_t* pem, uint8_t* out_key) {
    return _pem_x_ed_25519_read_key(pem,
                                    DER_OID_X25519,
                                    false,
                                    X25519_PRIVATE_KEY_RAW_LEN,
                                    out_key);
}

int8_t pem_read_x25519_public_key(const char_t* pem, uint8_t* out_key) {
    return _pem_x_ed_25519_read_key(pem,
                                    DER_OID_X25519,
                                    true,
                                    X25519_PUBLIC_KEY_RAW_LEN,
                                    out_key);
}

int8_t pem_read_ed25519_private_key(const char_t* pem, uint8_t* out_key) {
    return _pem_x_ed_25519_read_key(pem,
                                    DER_OID_ED25519,
                                    false,
                                    ED25519_PRIVATE_KEY_RAW_LEN,
                                    out_key);
}

int8_t pem_read_ed25519_public_key(const char_t* pem, uint8_t* out_key) {
    return _pem_x_ed_25519_read_key(pem,
                                    DER_OID_ED25519,
                                    true,
                                    ED25519_PUBLIC_KEY_RAW_LEN,
                                    out_key);
}

static int8_t _pem_x_ed_25519_write_key(const uint8_t* in_key, uint32_t in_key_len, uint64_t der_len,
                                        der_object_identifier_t oid,
                                        boolean_t is_pub_key, char_t** out_pem) {
    if (!in_key || !out_pem) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid arguments to _pem_write_key");
        return -1;
    }

    der_encoder_t* der_encoder = der_encoder_new();
    if (!der_encoder) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to create DER encoder");
        return -1;
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER sequence");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(!is_pub_key) { // For private key, encode version integer
        if(der_encoder_encode_integer(der_encoder, 0) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode DER integer");
            der_encoder_destroy(der_encoder);
            return -1;
        }
    }

    if(der_encoder_start_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER inner sequence");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_encode_object_identifier(der_encoder, oid) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode DER object identifier");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER inner sequence");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    if(is_pub_key) {
        if(der_encoder_encode_bit_string(der_encoder, in_key, in_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode DER bit string");
            der_encoder_destroy(der_encoder);
            return -1;
        }
    } else {
        if(der_encoder_start_octet_string(der_encoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to start DER octet string");
            der_encoder_destroy(der_encoder);
            return -1;
        }

        if(der_encoder_encode_octet_string(der_encoder, in_key, in_key_len) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode DER octet string");
            der_encoder_destroy(der_encoder);
            return -1;
        }

        if(der_encoder_end_octet_string(der_encoder) != 0) {
            PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER octet string");
            der_encoder_destroy(der_encoder);
            return -1;
        }
    }

    if(der_encoder_end_sequence(der_encoder) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to end DER sequence");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    uint64_t expected_der_len = 0;
    uint8_t* der_data = NULL;

    if(der_encoder_get_der_data(der_encoder, &der_data, &expected_der_len) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to get DER data from encoder");
        der_encoder_destroy(der_encoder);
        return -1;
    }

    der_encoder_destroy(der_encoder);

    if (expected_der_len != der_len) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Constructed DER length mismatch: expected %llu, got %llu",
                 der_len, expected_der_len);
        memory_free(der_data);
        return -1;
    }

    const char_t* label = is_pub_key ? "PUBLIC KEY" : "PRIVATE KEY";

    if(pem_encode(label, der_data, der_len, out_pem, NULL) != 0) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Failed to encode PEM key");
        return -1;
    }

    memory_free(der_data);

    return 0;
}

int8_t pem_write_x25519_private_key(const uint8_t* in_key, char_t** out_pem) {
    return _pem_x_ed_25519_write_key(in_key, X25519_PRIVATE_KEY_RAW_LEN, X25519_PRIVATE_KEY_DER_LEN,
                                     DER_OID_X25519, false, out_pem);
}

int8_t pem_write_x25519_public_key(const uint8_t* in_key, char_t** out_pem) {
    return _pem_x_ed_25519_write_key(in_key, X25519_PUBLIC_KEY_RAW_LEN, X25519_PUBLIC_KEY_DER_LEN,
                                     DER_OID_X25519, true, out_pem);
}

int8_t pem_write_ed25519_private_key(const uint8_t* in_key, char_t** out_pem) {
    return _pem_x_ed_25519_write_key(in_key, ED25519_PRIVATE_KEY_RAW_LEN, ED25519_PRIVATE_KEY_DER_LEN,
                                     DER_OID_ED25519, false, out_pem);
}

int8_t pem_write_ed25519_public_key(const uint8_t* in_key, char_t** out_pem) {
    return _pem_x_ed_25519_write_key(in_key, ED25519_PUBLIC_KEY_RAW_LEN, ED25519_PUBLIC_KEY_DER_LEN,
                                     DER_OID_ED25519, true, out_pem);
}
