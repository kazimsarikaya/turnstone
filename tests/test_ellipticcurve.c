/**
 * @file test_ellipticcurve.c
 * @brief Elliptic Curve Cryptography (ECC) Tests.
 *
 * Currently only secp256r1 implemented. hence only tests for that curve are included here. Future tests for other curves (e.g., secp384r1, secp521r1) may be added as needed.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <base64.h>
#include <bigint.h>
#include <strings.h>
#include <crypto/ellipticcurve.h>
#include <crypto/sha2.h>
#include <crypto/pem.h>
#include <crypto/der.h>

int32_t main(void);

static const char_t* private_sec256r1_key_in_pem =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIMy9vBDkkID/EBdI+OE7Z7FpWYTUQujRvHWzf68I9llRoAoGCCqGSM49\n"
    "AwEHoUQDQgAE47PToew+MMscfO6NBP88H5JWp/dz0gXZ//+jRfvTRLHWbHA9BYC/\n"
    "2nP0OnOBvd3YQyY1GnzaDCN9A4kYY9lX8A==\n"
    "-----END EC PRIVATE KEY-----\n";

static const char_t* public_sec256r1_key_in_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE47PToew+MMscfO6NBP88H5JWp/dz\n"
    "0gXZ//+jRfvTRLHWbHA9BYC/2nP0OnOBvd3YQyY1GnzaDCN9A4kYY9lX8A==\n"
    "-----END PUBLIC KEY-----\n";

static const uint8_t private_sec256r1_key_raw[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN] = {
    0xcc, 0xbd, 0xbc, 0x10, 0xe4, 0x90, 0x80, 0xff,
    0x10, 0x17, 0x48, 0xf8, 0xe1, 0x3b, 0x67, 0xb1,
    0x69, 0x59, 0x84, 0xd4, 0x42, 0xe8, 0xd1, 0xbc,
    0x75, 0xb3, 0x7f, 0xaf, 0x08, 0xf6, 0x59, 0x51
};

static const uint8_t public_sec256r1_key_raw[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN] = {
    0xe3, 0xb3, 0xd3, 0xa1, 0xec, 0x3e, 0x30, 0xcb,
    0x1c, 0x7c, 0xee, 0x8d, 0x04, 0xff, 0x3c, 0x1f,
    0x92, 0x56, 0xa7, 0xf7, 0x73, 0xd2, 0x05, 0xd9,
    0xff, 0xff, 0xa3, 0x45, 0xfb, 0xd3, 0x44, 0xb1,
    0xd6, 0x6c, 0x70, 0x3d, 0x05, 0x80, 0xbf, 0xda,
    0x73, 0xf4, 0x3a, 0x73, 0x81, 0xbd, 0xdd, 0xd8,
    0x43, 0x26, 0x35, 0x1a, 0x7c, 0xda, 0x0c, 0x23,
    0x7d, 0x03, 0x89, 0x18, 0x63, 0xd9, 0x57, 0xf0
};

static const uint8_t signature_sec256r1_to_verify[] = {
    0x60, 0x17, 0x01, 0x00, 0x25, 0x09, 0xd8, 0xd4,
    0x37, 0xdb, 0x7f, 0xc1, 0x38, 0x5b, 0x3e, 0x20,
    0xb0, 0xc7, 0x59, 0x6d, 0x95, 0x01, 0x98, 0x50,
    0x6d, 0x0b, 0xfd, 0x40, 0xb2, 0x95, 0x2e, 0x8a,
    0xab, 0xbf, 0xf0, 0xdd, 0xe7, 0x96, 0x5b, 0x5d,
    0x94, 0xab, 0xa2, 0xfb, 0x72, 0xb7, 0x30, 0xe1,
    0x2b, 0x44, 0x31, 0x5a, 0x15, 0xf7, 0xdf, 0xb4,
    0xa7, 0x3f, 0x40, 0x73, 0x1f, 0x6e, 0x3f, 0x8a
};

_Static_assert(sizeof(signature_sec256r1_to_verify) == ELLIPTICCURVE_SECP256R1_SIGNATURE_RAW_LEN, "Signature size mismatch");

static const char_t* private_secp384r1_key_in_pem =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MIGkAgEBBDDT2B33w4zEumGn/4d2P6ylHYUKv5BGUwzV1w1sPQnhHz54FBTVNhnj\n"
    "KUwVdjSnC8ugBwYFK4EEACKhZANiAAQpu0NhjiSL8PTVInUafDuaSs0OdPsh66mV\n"
    "K1wVFzMzKlTahvdDwbC1CEdw0UMxYjqeCF6Tc5CtUZr5p+xugG571D4YAjFkWKBD\n"
    "azR9PLZvW7S7HAsewn+A4zsgsh1Z1GI=\n"
    "-----END EC PRIVATE KEY-----\n";

static const char_t* public_secp384r1_key_in_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEKbtDYY4ki/D01SJ1Gnw7mkrNDnT7Ieup\n"
    "lStcFRczMypU2ob3Q8GwtQhHcNFDMWI6nghek3OQrVGa+afsboBue9Q+GAIxZFig\n"
    "Q2s0fTy2b1u0uxwLHsJ/gOM7ILIdWdRi\n"
    "-----END PUBLIC KEY-----\n";

static const uint8_t private_secp384r1_key_raw[ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN] = {
    0xd3, 0xd8, 0x1d, 0xf7, 0xc3, 0x8c, 0xc4, 0xba,
    0x61, 0xa7, 0xff, 0x87, 0x76, 0x3f, 0xac, 0xa5,
    0x1d, 0x85, 0x0a, 0xbf, 0x90, 0x46, 0x53, 0x0c,
    0xd5, 0xd7, 0x0d, 0x6c, 0x3d, 0x09, 0xe1, 0x1f,
    0x3e, 0x78, 0x14, 0x14, 0xd5, 0x36, 0x19, 0xe3,
    0x29, 0x4c, 0x15, 0x76, 0x34, 0xa7, 0x0b, 0xcb,
};

static const uint8_t public_secp384r1_key_raw[ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN] = {
    0x29, 0xbb, 0x43, 0x61, 0x8e, 0x24, 0x8b, 0xf0,
    0xf4, 0xd5, 0x22, 0x75, 0x1a, 0x7c, 0x3b, 0x9a,
    0x4a, 0xcd, 0x0e, 0x74, 0xfb, 0x21, 0xeb, 0xa9,
    0x95, 0x2b, 0x5c, 0x15, 0x17, 0x33, 0x33, 0x2a,
    0x54, 0xda, 0x86, 0xf7, 0x43, 0xc1, 0xb0, 0xb5,
    0x08, 0x47, 0x70, 0xd1, 0x43, 0x31, 0x62, 0x3a,
    0x9e, 0x08, 0x5e, 0x93, 0x73, 0x90, 0xad, 0x51,
    0x9a, 0xf9, 0xa7, 0xec, 0x6e, 0x80, 0x6e, 0x7b,
    0xd4, 0x3e, 0x18, 0x02, 0x31, 0x64, 0x58, 0xa0,
    0x43, 0x6b, 0x34, 0x7d, 0x3c, 0xb6, 0x6f, 0x5b,
    0xb4, 0xbb, 0x1c, 0x0b, 0x1e, 0xc2, 0x7f, 0x80,
    0xe3, 0x3b, 0x20, 0xb2, 0x1d, 0x59, 0xd4, 0x62,
};

int32_t main(void) {
    int8_t res;
    uint8_t private_sec256r1_key_raw_local[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN] = {0};
    res = pem_read_secp256r1_private_key(private_sec256r1_key_in_pem, private_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to read private key from PEM\n");
        return -1;
    }

    // print the raw private key
    printf("Raw Private Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN; i++) {
        printf("%02x", private_sec256r1_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(private_sec256r1_key_raw_local, private_sec256r1_key_raw, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) != 0) {
        print_error("Private key does not match expected raw value\n");
        return -1;
    }

    print_success("Private key successfully read and verified\n");

    char_t* private_sec256r1_key_out_pem = NULL;
    res = pem_write_secp256r1_private_key(private_sec256r1_key_raw_local, &private_sec256r1_key_out_pem);

    if (res != 0) {
        print_error("Failed to write private key to PEM\n");
        return -1;
    }

    if(strlen(private_sec256r1_key_out_pem) != strlen(private_sec256r1_key_in_pem) ||
       memory_memcompare(private_sec256r1_key_out_pem, private_sec256r1_key_in_pem, strlen(private_sec256r1_key_in_pem)) != 0) {
        print_error("Written private key PEM does not match original\n");
        memory_free(private_sec256r1_key_out_pem);
        return -1;
    }

    print_success("Private key successfully written to PEM and verified\n");
    memory_free(private_sec256r1_key_out_pem);

    // read public key from PEM
    uint8_t public_sec256r1_key_raw_local[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    res = pem_read_secp256r1_public_key(public_sec256r1_key_in_pem, public_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to read public key from PEM\n");
        return -1;
    }

    // print the raw public key
    printf("Raw Public Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN; i++) {
        printf("%02x", public_sec256r1_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(public_sec256r1_key_raw_local, public_sec256r1_key_raw, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN) != 0) {
        print_error("Public key does not match expected raw value\n");
        return -1;
    }

    print_success("Public key successfully read and verified\n");

    char_t* public_sec256r1_key_out_pem = NULL;
    res = pem_write_secp256r1_public_key(public_sec256r1_key_raw_local, &public_sec256r1_key_out_pem);

    if (res != 0) {
        print_error("Failed to write public key to PEM\n");
        return -1;
    }

    if(strlen(public_sec256r1_key_out_pem) != strlen(public_sec256r1_key_in_pem) ||
       memory_memcompare(public_sec256r1_key_out_pem, public_sec256r1_key_in_pem, strlen(public_sec256r1_key_in_pem)) != 0) {
        print_error("Written public key PEM does not match original\n");
        memory_free(public_sec256r1_key_out_pem);
        return -1;
    }

    print_success("Public key successfully written to PEM and verified\n");
    memory_free(public_sec256r1_key_out_pem);

    const char_t* message = "Test message for signing";
    uint8_t signature[64];
    res = ellipticcurve_secp256r1_sign(signature, (const uint8_t*)message, strlen(message), private_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to sign message with secp256r1\n");
        return -1;
    }

    printf("secp256r1 Signature:\n");
    for (size_t i = 0; i < 64; i++) {
        printf("%02x", signature[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    res = ellipticcurve_secp256r1_verify(signature, (const uint8_t*)message, strlen(message), public_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to verify secp256r1 signature\n");
        return -1;
    }

    print_success("secp256r1 signature successfully verified\n");

    res = ellipticcurve_secp256r1_verify(signature_sec256r1_to_verify, (const uint8_t*)message, strlen(message), public_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to verify known signature with secp256r1\n");
        return -1;
    }

    print_success("Known signature successfully verified with secp256r1\n");

    uint8_t second_public_sec256r1_key_raw[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    uint8_t second_private_sec256r1_key_raw[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];

    res = ellipticcurve_secp256r1_generate_keypair(second_private_sec256r1_key_raw, second_public_sec256r1_key_raw);

    if (res != 0) {
        print_error("Failed to generate secp256r1 keypair\n");
        return -1;
    }

    uint8_t shared_secret1[ELLIPTICCURVE_SECP256R1_SHARED_SECRET_RAW_LEN];
    uint8_t shared_secret2[ELLIPTICCURVE_SECP256R1_SHARED_SECRET_RAW_LEN];

    res = ellipticcurve_secp256r1_shared_secret(shared_secret1, private_sec256r1_key_raw_local, second_public_sec256r1_key_raw);

    if (res != 0) {
        print_error("Failed to compute shared secret (1)\n");
        return -1;
    }

    res = ellipticcurve_secp256r1_shared_secret(shared_secret2, second_private_sec256r1_key_raw, public_sec256r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to compute shared secret (2)\n");
        return -1;
    }

    if (memory_memcompare(shared_secret1, shared_secret2, 32) != 0) {
        print_error("Shared secrets do not match\n");
        return -1;
    }

    print_success("Shared secret successfully computed and verified between both parties\n");

    uint8_t private_sec384r1_key_raw_local[ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN] = {0};

    res = pem_read_secp384r1_private_key(private_secp384r1_key_in_pem, private_sec384r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to read secp384r1 private key from PEM\n");
        return -1;
    }

    printf("Raw secp384r1 Private Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN; i++) {
        printf("%02x", private_sec384r1_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(private_sec384r1_key_raw_local, private_secp384r1_key_raw, ELLIPTICCURVE_SECP384R1_PRIVATE_KEY_RAW_LEN) != 0) {
        print_error("secp384r1 Private key does not match expected raw value\n");
        return -1;
    }

    print_success("secp384r1 Private key successfully read and verified\n");

    uint8_t public_sec384r1_key_raw_local[ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN];

    res = pem_read_secp384r1_public_key(public_secp384r1_key_in_pem, public_sec384r1_key_raw_local);

    if (res != 0) {
        print_error("Failed to read secp384r1 public key from PEM\n");
        return -1;
    }

    printf("Raw secp384r1 Public Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN; i++) {
        printf("%02x", public_sec384r1_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(public_sec384r1_key_raw_local, public_secp384r1_key_raw, ELLIPTICCURVE_SECP384R1_PUBLIC_KEY_RAW_LEN) != 0) {
        print_error("secp384r1 Public key does not match expected raw value\n");
        return -1;
    }

    print_success("secp384r1 Public key successfully read and verified\n");


    return 0;
}
