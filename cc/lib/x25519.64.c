/**
 * @file x25519.64.c
 * @brief X25519 Key Exchange Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <x25519.h>
#include <bigint.h>
#include <memory.h>
#include <random.h>
#include <logging.h>
#include <strings.h>
#include <base64.h>

MODULE("turnstone.lib.crypto");


int8_t x25519_shared_secret(uint8_t       out_shared[X25519_SHARED_SECRET_LEN],
                            const uint8_t my_priv[X25519_PRIVATE_KEY_RAW_LEN],
                            const uint8_t their_pub[X25519_PUBLIC_KEY_RAW_LEN]) {
    if (!out_shared || !my_priv || !their_pub) {
        return -1;
    }

    // 1. Prepare my private key (Scalar)
    uint8_t k[X25519_PRIVATE_KEY_RAW_LEN];
    memory_memcopy(my_priv, k, X25519_PRIVATE_KEY_RAW_LEN);
    x25519_clamp(k); // Clamping (Bit 0,1,2=0; Bit 255=0; Bit 254=1)

    // 2. Constants and Modulus
    bigint_t * p = bigint_create();
    bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");

    bigint_t * u_peer = bigint_create();
    bigint_from_bytes_le(u_peer, their_pub, 32);

    // 3. Initialize Ladder State
    // R0 = Identity (1:0), R1 = (u_peer : 1)
    bigint_t * x0 = bigint_one();
    bigint_t * z0 = bigint_zero();
    bigint_t * x1 = bigint_clone(u_peer);
    bigint_t * z1 = bigint_one();

    // Temporary variables for the ladder math
    bigint_t * da = bigint_create(), * db = bigint_create();
    bigint_t * dc = bigint_create(), * dd = bigint_create();
    bigint_t * e  = bigint_create(), * f  = bigint_create();
    bigint_t * g  = bigint_create(), * h  = bigint_create();

    const uint64_t a24 = 121666;

    // 4. The Montgomery Ladder Loop
    for (int64_t i = 254; i >= 0; i--) {
        uint8_t bit = (k[i >> 3] >> (i & 7)) & 1;

        // Conditional Swap
        if (bit) {
            bigint_t* t;
            t = x0; x0 = x1; x1 = t;
            t = z0; z0 = z1; z1 = t;
        }

        // --- Point Double and Differential Add ---
        bigint_add_mod(da, x0, z0, p); // da = x0 + z0
        bigint_sub_mod(db, x0, z0, p); // db = x0 - z0
        bigint_add_mod(dc, x1, z1, p); // dc = x1 + z1
        bigint_sub_mod(dd, x1, z1, p); // dd = x1 - z1

        bigint_mul_mod(e, dd, da, p); // e = (x1-z1)(x0+z0)
        bigint_mul_mod(f, dc, db, p); // f = (x1+z1)(x0-z0)

        // x1 = (e + f)^2
        bigint_add_mod(g, e, f, p);
        bigint_mul_mod(x1, g, g, p);

        // z1 = u_peer * (e - f)^2  <-- IMPORTANT: Peer's PubKey instead of 9
        bigint_sub_mod(g, e, f, p);
        bigint_mul_mod(h, g, g, p);
        bigint_mul_mod(z1, h, u_peer, p);

        // AA = da^2, BB = db^2, E = AA - BB
        bigint_mul_mod(e, da, da, p); // e = AA
        bigint_mul_mod(f, db, db, p); // f = BB
        bigint_sub_mod(g, e, f, p); // g = E

        // x0 = AA * BB
        bigint_mul_mod(x0, e, f, p);

        // z0 = E * (BB + a24 * E)
        bigint_set_bigint(h, g);
        bigint_mul_uint64(h, a24);
        bigint_mod(h, h, p);
        bigint_add_mod(h, h, f, p);
        bigint_mul_mod(z0, g, h, p);

        // Swap back
        if (bit) {
            bigint_t* t;
            t = x0; x0 = x1; x1 = t;
            t = z0; z0 = z1; z1 = t;
        }
    }

    // 5. Finalize: x = x0 * inv(z0) mod p
    bigint_t * inv_z0 = bigint_create();
    if (bigint_mod_inv(inv_z0, z0, p) == 0) {
        bigint_mul_mod(x0, x0, inv_z0, p);
        bigint_to_bytes_le(x0, out_shared, X25519_SHARED_SECRET_LEN);
    } else {
        // This happens if z0 is zero (invalid public key/point at infinity)
        memory_memset(out_shared, 0, X25519_SHARED_SECRET_LEN);
    }

    // 6. Mandatory Cleanup
    bigint_destroy(p); bigint_destroy(u_peer);
    bigint_destroy(x0); bigint_destroy(z0);
    bigint_destroy(x1); bigint_destroy(z1);
    bigint_destroy(da); bigint_destroy(db);
    bigint_destroy(dc); bigint_destroy(dd);
    bigint_destroy(e);  bigint_destroy(f);
    bigint_destroy(g);  bigint_destroy(h);
    bigint_destroy(inv_z0);

    return 0;
}

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

int8_t x25519_derive_public(uint8_t out_pub[X25519_PUBLIC_KEY_RAW_LEN], const uint8_t in_priv[X25519_PRIVATE_KEY_RAW_LEN]) {
    // 1. Prepare Scalar (Clamping)
    uint8_t k[32];
    memory_memcopy(in_priv, k, 32);
    k[0] &= 248;
    k[31] &= 127;
    k[31] |= 64;

    // 2. Constants and State
    bigint_t * p = bigint_create();
    bigint_set_str(p, "7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFED");

    // Projective coordinates for R0 (initialized to Identity) and R1 (Base Point)
    bigint_t * x0 = bigint_one(); // X0
    bigint_t * z0 = bigint_zero(); // Z0
    bigint_t* x1 = bigint_create();
    bigint_set_uint64(x1, 9); // X1 (Base x=9)
    bigint_t * z1 = bigint_one(); // Z1

    // Temp variables for the ladder (A, B, C, D, E...)
    bigint_t * da = bigint_create(), * db = bigint_create();
    bigint_t * dc = bigint_create(), * dd = bigint_create();
    bigint_t * e  = bigint_create(), * f  = bigint_create();
    bigint_t * g  = bigint_create(), * h  = bigint_create();

    const uint64_t a24 = 121666;

    // 3. The Montgomery Ladder
    for (int64_t i = 254; i >= 0; i--) {
        uint8_t bit = (k[i >> 3] >> (i & 7)) & 1;

        // Conditional Swap based on bit
        if (bit) {
            bigint_t* tmp;
            tmp = x0; x0 = x1; x1 = tmp;
            tmp = z0; z0 = z1; z1 = tmp;
        }

        // Formulas for Montgomery Ladder (X-only)
        // A = x0 + z0
        bigint_add_mod(da, x0, z0, p);
        // B = x0 - z0
        bigint_sub_mod(db, x0, z0, p);
        // C = x1 + z1
        bigint_add_mod(dc, x1, z1, p);
        // D = x1 - z1
        bigint_sub_mod(dd, x1, z1, p);

        // DA = D * A
        bigint_mul_mod(e, dd, da, p);
        // CB = C * B
        bigint_mul_mod(f, dc, db, p);

        // x1 = (DA + CB)^2
        bigint_add_mod(g, e, f, p);
        bigint_mul_mod(x1, g, g, p);
        // z1 = 9 * (DA - CB)^2  (Note: 9 is the original x-coordinate)
        bigint_sub_mod(g, e, f, p);
        bigint_mul_mod(h, g, g, p);
        bigint_set_bigint(z1, h);
        bigint_mul_uint64(z1, 9);
        bigint_mod(z1, z1, p);

        // AA = A^2
        bigint_mul_mod(e, da, da, p);
        // BB = B^2
        bigint_mul_mod(f, db, db, p);
        // E = AA - BB
        bigint_sub_mod(g, e, f, p);

        // x0 = AA * BB
        bigint_mul_mod(x0, e, f, p);
        // z0 = E * (BB + a24 * E)
        bigint_set_bigint(h, g);
        bigint_mul_uint64(h, a24);
        bigint_mod(h, h, p);
        bigint_add_mod(h, h, f, p);
        bigint_mul_mod(z0, g, h, p);

        if (bit) {
            bigint_t* tmp;
            tmp = x0; x0 = x1; x1 = tmp;
            tmp = z0; z0 = z1; z1 = tmp;
        }
    }

    // 4. Convert back to Affine: x = x0 / z0
    bigint_t * inv_z0 = bigint_create();
    bigint_mod_inv(inv_z0, z0, p);
    bigint_mul_mod(x0, x0, inv_z0, p);

    // 5. Serialize
    bigint_to_bytes_le(x0, out_pub, X25519_PUBLIC_KEY_RAW_LEN);

    // 6. Cleanup (Essential for security!)
    // Destroy all bigints...
    bigint_destroy(p);
    bigint_destroy(x0);
    bigint_destroy(z0);
    bigint_destroy(x1);
    bigint_destroy(z1);
    bigint_destroy(da);
    bigint_destroy(db);
    bigint_destroy(dc);
    bigint_destroy(dd);
    bigint_destroy(e);
    bigint_destroy(f);
    bigint_destroy(g);
    bigint_destroy(h);
    bigint_destroy(inv_z0);

    return 0;
}


void x25519_clamp(uint8_t k[X25519_PRIVATE_KEY_RAW_LEN]) {
    k[0] &= 248; // Clear bits 0, 1, 2
    k[31] &= 127; // Clear bit 255
    k[31] |= 64; // Set bit 254
}

int8_t pem_read_x25519_private_key(const char_t* pem, uint8_t* out_key) {
    const char_t* header = "-----BEGIN PRIVATE KEY-----\n";
    const char_t* footer = "-----END PRIVATE KEY-----\n";
    const char_t* start = strstr(pem, header);

    if (!start) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Private key PEM header not found\n");
        return -1;
    }

    start += strlen(header);
    const char_t* end = strstr(pem, footer);

    if (!end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Private key PEM footer not found\n");
        return -1;
    }

    size_t b64_len = end - start;
    uint8_t* b64_data = (uint8_t*)memory_malloc(b64_len + 1);

    if (!b64_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for base64 data\n");
        return -1;
    }

    memory_memcopy((const uint8_t*)start, b64_data, b64_len);

    uint8_t* decoded_data = NULL;

    size_t decoded_len = base64_decode(b64_data, b64_len, &decoded_data);

    memory_free(b64_data);

    if (decoded_len != X25519_PRIVATE_KEY_DER_LEN) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decoded private key length mismatch: expected %d, got %llu\n", X25519_PRIVATE_KEY_DER_LEN, decoded_len);
        return -1;
    }

    // parse DER structure to extract raw key
    // DER structure for PKCS#8 private key:
    // SEQUENCE {
    // INTEGER (0)
    // SEQUENCE {
    // OBJECT IDENTIFIER (id-X25519)
    // }
    // OCTET STRING (private key)
    // }
    uint8_t* der = decoded_data;
    if (der[0] != 0x30) { // SEQUENCE
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected SEQUENCE\n");
        memory_free(decoded_data);
        return -1;
    }

    // Skip to OCTET STRING
    size_t index = 2; // Skip SEQUENCE tag and length
    if (der[index] != 0x02) { // INTEGER
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected INTEGER\n");
        memory_free(decoded_data);
        return -1;
    }
    index += 2 + der[index + 1]; // Skip INTEGER
    if (der[index] != 0x30) { // SEQUENCE
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected SEQUENCE\n");
        memory_free(decoded_data);
        return -1;
    }
    index += 2 + der[index + 1]; // Skip SEQUENCE
    if (der[index] != 0x04) { // OCTET STRING
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected OCTET STRING\n");
        memory_free(decoded_data);
        return -1;
    }
    size_t octet_len = der[index + 1];
    if (octet_len != X25519_PRIVATE_KEY_RAW_LEN + 2) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid OCTET STRING length: expected %d, got %llu\n", X25519_PRIVATE_KEY_RAW_LEN + 2, octet_len);
        memory_free(decoded_data);
        return -1;
    }

    // The actual private key is inside the OCTET STRING, skipping 2 bytes
    memory_memcopy(der + index + 2 + 2, out_key, X25519_PRIVATE_KEY_RAW_LEN);
    memory_free(decoded_data);

    return 0;
}

int8_t pem_read_x25519_public_key(const char_t* pem, uint8_t* out_key) {
    const char_t* header = "-----BEGIN PUBLIC KEY-----\n";
    const char_t* footer = "-----END PUBLIC KEY-----\n";
    const char_t* start = strstr(pem, header);

    if (!start) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Public key PEM header not found\n");
        return -1;
    }

    start += strlen(header);
    const char_t* end = strstr(pem, footer);

    if (!end) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Public key PEM footer not found\n");
        return -1;
    }

    size_t b64_len = end - start;
    uint8_t* b64_data = (uint8_t*)memory_malloc(b64_len + 1);

    if (!b64_data) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for base64 data\n");
        return -1;
    }

    memory_memcopy((const uint8_t*)start, b64_data, b64_len);

    uint8_t* decoded_data = NULL;

    size_t decoded_len = base64_decode(b64_data, b64_len, &decoded_data);

    memory_free(b64_data);

    if (decoded_len != X25519_PUBLIC_KEY_DER_LEN) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Decoded public key length mismatch: expected %d, got %llu\n", X25519_PUBLIC_KEY_DER_LEN, decoded_len);
        return -1;
    }

    // parse DER structure to extract raw key
    // DER structure for SubjectPublicKeyInfo:
    // SEQUENCE {
    // SEQUENCE {
    // OBJECT IDENTIFIER (id-X25519)
    // }
    // BIT STRING (public key)
    // }
    uint8_t* der = decoded_data;
    if (der[0] != 0x30) { // SEQUENCE
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected SEQUENCE\n");
        memory_free(decoded_data);
        return -1;
    }
    // Skip to BIT STRING
    size_t index = 2; // Skip SEQUENCE tag and length
    if (der[index] != 0x30) { // SEQUENCE
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected SEQUENCE\n");
        memory_free(decoded_data);
        return -1;
    }
    index += 2 + der[index + 1]; // Skip SEQUENCE
    if (der[index] != 0x03) { // BIT STRING
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid DER format: expected BIT STRING\n");
        memory_free(decoded_data);
        return -1;
    }
    size_t bitstr_len = der[index + 1];
    if (bitstr_len != X25519_PUBLIC_KEY_RAW_LEN + 1) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Invalid BIT STRING length: expected %d, got %llu\n", X25519_PUBLIC_KEY_RAW_LEN + 1, bitstr_len);
        memory_free(decoded_data);
        return -1;
    }

    // The actual public key is inside the BIT STRING, skipping 1 byte
    memory_memcopy(der + index + 2 + 1, out_key, X25519_PUBLIC_KEY_RAW_LEN);
    memory_free(decoded_data);

    return 0;
}

int8_t pem_write_x25519_private_key(const uint8_t* in_key, char_t** out_pem) {
    buffer_t* der_buf = buffer_new();
    if (!der_buf) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for DER buffer\n");
        return -1;
    }

    // Construct DER structure for PKCS#8 private key
    // SEQUENCE {
    buffer_append_byte(der_buf, 0x30); // SEQUENCE
    buffer_append_byte(der_buf, 46); // Length
    // INTEGER (0)
    buffer_append_byte(der_buf, 0x02); // INTEGER
    buffer_append_byte(der_buf, 1); // Length
    buffer_append_byte(der_buf, 0x00); // Value
    // SEQUENCE {
    buffer_append_byte(der_buf, 0x30); // SEQUENCE
    buffer_append_byte(der_buf, 5); // Length
    // OBJECT IDENTIFIER (id-X25519)
    buffer_append_byte(der_buf, 0x06); // OBJECT IDENTIFIER
    buffer_append_byte(der_buf, 3); // Length
    buffer_append_byte(der_buf, 0x2b); // 1.3
    buffer_append_byte(der_buf, 0x65); // 101
    buffer_append_byte(der_buf, 0x6e); // 110
    // OCTET STRING (private key)
    buffer_append_byte(der_buf, 0x04); // OCTET STRING
    buffer_append_byte(der_buf, 34); // Length
    buffer_append_byte(der_buf, 0x04); // OCTET STRING inside
    buffer_append_byte(der_buf, 32); // Length of raw key
    buffer_append_bytes(der_buf, (uint8_t*)in_key, X25519_PRIVATE_KEY_RAW_LEN);
    // }
    // }
    // }
    // Base64 encode DER
    uint64_t expected_der_len = 0;
    uint8_t* der_data = buffer_get_all_bytes_and_destroy(der_buf, &expected_der_len);

    if (expected_der_len != X25519_PRIVATE_KEY_DER_LEN) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Constructed DER length mismatch: expected %d, got %llu\n", X25519_PRIVATE_KEY_DER_LEN, expected_der_len);
        memory_free(der_data);
        return -1;
    }

    uint8_t* b64_encoded = NULL;
    size_t b64_len = base64_encode(der_data, expected_der_len, true, &b64_encoded);
    memory_free(der_data);

    // Format PEM
    buffer_t* pem_buf = buffer_new();
    if (!pem_buf) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for PEM buffer\n");
        memory_free(b64_encoded);
        return -1;
    }

    const char_t* header = "-----BEGIN PRIVATE KEY-----\n";
    const char_t* footer = "-----END PRIVATE KEY-----\n";

    buffer_append_bytes(pem_buf, (uint8_t*)header, strlen(header));
    for (size_t i = 0; i < b64_len; i += 64) {
        size_t line_len = (b64_len - i > 64) ? 64 : (b64_len - i);
        buffer_append_bytes(pem_buf, (uint8_t*)(b64_encoded + i), line_len);
        buffer_append_byte(pem_buf, '\n');
    }
    buffer_append_bytes(pem_buf, (uint8_t*)footer, strlen(footer));

    memory_free(b64_encoded);

    *out_pem = (char_t*)buffer_get_all_bytes_and_destroy(pem_buf, NULL);

    return 0;
}

int8_t pem_write_x25519_public_key(const uint8_t* in_key, char_t** out_pem) {
    buffer_t* der_buf = buffer_new();
    if (!der_buf) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for DER buffer\n");
        return -1;
    }

    // Construct DER structure for SubjectPublicKeyInfo
    // SEQUENCE { (Total length: 42 bytes = 0x2A)
    buffer_append_byte(der_buf, 0x30);
    buffer_append_byte(der_buf, 42); // Corrected: 0x2A (Total inner payload)

    // SEQUENCE { (Algorithm Identifier length: 5 bytes)
    buffer_append_byte(der_buf, 0x30);
    buffer_append_byte(der_buf, 5);
    // OBJECT IDENTIFIER (id-X25519)
    buffer_append_byte(der_buf, 0x06);
    buffer_append_byte(der_buf, 3);
    buffer_append_byte(der_buf, 0x2b);
    buffer_append_byte(der_buf, 0x65);
    buffer_append_byte(der_buf, 0x6e);
    // } (End Algorithm Sequence)

    // BIT STRING (public key)
    buffer_append_byte(der_buf, 0x03);
    buffer_append_byte(der_buf, 33); // Corrected: 1 byte (unused bits) + 32 bytes (key) = 33 (0x21)
    buffer_append_byte(der_buf, 0x00); // Unused bits byte
    buffer_append_bytes(der_buf, (uint8_t*)in_key, 32);
    // } (End Outer Sequence)

    uint64_t expected_der_len = 0;
    uint8_t* der_data = buffer_get_all_bytes_and_destroy(der_buf, &expected_der_len);

    if (expected_der_len != X25519_PUBLIC_KEY_DER_LEN) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Constructed DER length mismatch: expected %d, got %llu\n", X25519_PUBLIC_KEY_DER_LEN, expected_der_len);
        memory_free(der_data);
        return -1;
    }

    uint8_t* b64_encoded = NULL;
    size_t b64_len = base64_encode(der_data, expected_der_len, true, &b64_encoded);
    memory_free(der_data);

    // Format PEM
    buffer_t* pem_buf = buffer_new();
    if (!pem_buf) {
        PRINTLOG(CRYPTOLIB, LOG_ERROR, "Memory allocation failed for PEM buffer\n");
        memory_free(b64_encoded);
        return -1;
    }

    const char_t* header = "-----BEGIN PUBLIC KEY-----\n";
    const char_t* footer = "-----END PUBLIC KEY-----\n";

    buffer_append_bytes(pem_buf, (uint8_t*)header, strlen(header));
    for (size_t i = 0; i < b64_len; i += 64) {
        size_t line_len = (b64_len - i > 64) ? 64 : (b64_len - i);
        buffer_append_bytes(pem_buf, (uint8_t*)(b64_encoded + i), line_len);
        buffer_append_byte(pem_buf, '\n');
    }
    buffer_append_bytes(pem_buf, (uint8_t*)footer, strlen(footer));

    memory_free(b64_encoded);

    *out_pem = (char_t*)buffer_get_all_bytes_and_destroy(pem_buf, NULL);

    return 0;
}
