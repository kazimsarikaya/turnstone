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

const char_t* private_key_in_pem =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIMy9vBDkkID/EBdI+OE7Z7FpWYTUQujRvHWzf68I9llRoAoGCCqGSM49\n"
    "AwEHoUQDQgAE47PToew+MMscfO6NBP88H5JWp/dz0gXZ//+jRfvTRLHWbHA9BYC/\n"
    "2nP0OnOBvd3YQyY1GnzaDCN9A4kYY9lX8A==\n"
    "-----END EC PRIVATE KEY-----\n";

const char_t* public_key_in_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE47PToew+MMscfO6NBP88H5JWp/dz\n"
    "0gXZ//+jRfvTRLHWbHA9BYC/2nP0OnOBvd3YQyY1GnzaDCN9A4kYY9lX8A==\n"
    "-----END PUBLIC KEY-----\n";

const uint8_t private_key_raw[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN] = {
    0xcc, 0xbd, 0xbc, 0x10, 0xe4, 0x90, 0x80, 0xff,
    0x10, 0x17, 0x48, 0xf8, 0xe1, 0x3b, 0x67, 0xb1,
    0x69, 0x59, 0x84, 0xd4, 0x42, 0xe8, 0xd1, 0xbc,
    0x75, 0xb3, 0x7f, 0xaf, 0x08, 0xf6, 0x59, 0x51
};

const uint8_t public_key_raw[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN] = {
    0xe3, 0xb3, 0xd3, 0xa1, 0xec, 0x3e, 0x30, 0xcb,
    0x1c, 0x7c, 0xee, 0x8d, 0x04, 0xff, 0x3c, 0x1f,
    0x92, 0x56, 0xa7, 0xf7, 0x73, 0xd2, 0x05, 0xd9,
    0xff, 0xff, 0xa3, 0x45, 0xfb, 0xd3, 0x44, 0xb1,
    0xd6, 0x6c, 0x70, 0x3d, 0x05, 0x80, 0xbf, 0xda,
    0x73, 0xf4, 0x3a, 0x73, 0x81, 0xbd, 0xdd, 0xd8,
    0x43, 0x26, 0x35, 0x1a, 0x7c, 0xda, 0x0c, 0x23,
    0x7d, 0x03, 0x89, 0x18, 0x63, 0xd9, 0x57, 0xf0
};

int32_t main(void) {
    int8_t res;
    uint8_t private_key_raw_local[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];
    res = pem_read_secp256r1_private_key(private_key_in_pem, private_key_raw_local);

    if (res != 0) {
        print_error("Failed to read private key from PEM\n");
        return -1;
    }

    // print the raw private key
    printf("Raw Private Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN; i++) {
        printf("%02x", private_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(private_key_raw_local, private_key_raw, ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN) != 0) {
        print_error("Private key does not match expected raw value\n");
        return -1;
    }

    print_success("Private key successfully read and verified\n");

    char_t* private_key_out_pem = NULL;
    res = pem_write_secp256r1_private_key(private_key_raw_local, &private_key_out_pem);

    if (res != 0) {
        print_error("Failed to write private key to PEM\n");
        return -1;
    }

    if(strlen(private_key_out_pem) != strlen(private_key_in_pem) ||
       memory_memcompare(private_key_out_pem, private_key_in_pem, strlen(private_key_in_pem)) != 0) {
        print_error("Written private key PEM does not match original\n");
        memory_free(private_key_out_pem);
        return -1;
    }

    print_success("Private key successfully written to PEM and verified\n");
    memory_free(private_key_out_pem);

    // read public key from PEM
    uint8_t public_key_raw_local[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    res = pem_read_secp256r1_public_key(public_key_in_pem, public_key_raw_local);

    if (res != 0) {
        print_error("Failed to read public key from PEM\n");
        return -1;
    }

    // print the raw public key
    printf("Raw Public Key:\n");
    for (size_t i = 0; i < ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN; i++) {
        printf("%02x", public_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(public_key_raw_local, public_key_raw, ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN) != 0) {
        print_error("Public key does not match expected raw value\n");
        return -1;
    }

    print_success("Public key successfully read and verified\n");

    char_t* public_key_out_pem = NULL;
    res = pem_write_secp256r1_public_key(public_key_raw_local, &public_key_out_pem);

    if (res != 0) {
        print_error("Failed to write public key to PEM\n");
        return -1;
    }

    if(strlen(public_key_out_pem) != strlen(public_key_in_pem) ||
       memory_memcompare(public_key_out_pem, public_key_in_pem, strlen(public_key_in_pem)) != 0) {
        print_error("Written public key PEM does not match original\n");
        memory_free(public_key_out_pem);
        return -1;
    }

    print_success("Public key successfully written to PEM and verified\n");
    memory_free(public_key_out_pem);

    const char_t* message = "Test message for signing";
    uint8_t signature[64];
    res = ellipticcurve_secp256r1_sign(signature, (const uint8_t*)message, strlen(message), private_key_raw_local);

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

    res = ellipticcurve_secp256r1_verify(signature, (const uint8_t*)message, strlen(message), public_key_raw_local);

    if (res != 0) {
        print_error("Failed to verify secp256r1 signature\n");
        return -1;
    }

    print_success("secp256r1 signature successfully verified\n");

    uint8_t second_public_key_raw[ELLIPTICCURVE_SECP256R1_PUBLIC_KEY_RAW_LEN];
    uint8_t second_private_key_raw[ELLIPTICCURVE_SECP256R1_PRIVATE_KEY_RAW_LEN];

    res = ellipticcurve_secp256r1_generate_keypair(second_private_key_raw, second_public_key_raw);

    if (res != 0) {
        print_error("Failed to generate secp256r1 keypair\n");
        return -1;
    }

    uint8_t shared_secret1[32];
    uint8_t shared_secret2[32];

    res = ellipticcurve_secp256r1_compute_shared_secret(shared_secret1, private_key_raw_local, second_public_key_raw);

    if (res != 0) {
        print_error("Failed to compute shared secret (1)\n");
        return -1;
    }

    res = ellipticcurve_secp256r1_compute_shared_secret(shared_secret2, second_private_key_raw, public_key_raw_local);

    if (res != 0) {
        print_error("Failed to compute shared secret (2)\n");
        return -1;
    }

    if (memory_memcompare(shared_secret1, shared_secret2, 32) != 0) {
        print_error("Shared secrets do not match\n");
        return -1;
    }

    print_success("Shared secret successfully computed and verified between both parties\n");

    return 0;
}
