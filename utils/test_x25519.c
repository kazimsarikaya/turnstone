/**
 * @file test_x25519.c
 * @brief X25519 Tests
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <base64.h>
#include <bigint.h>
#include <strings.h>
#include <x25519.h>

int main(void);

const char_t* private_key_in_pem =
    "-----BEGIN PRIVATE KEY-----\n"
    "MC4CAQAwBQYDK2VuBCIEIGi/KmXkJH3Dx4oPgCxSiMQf7IVwPL3MsCd0/ztZ71ZL\n"
    "-----END PRIVATE KEY-----\n";

const uint8_t private_key_raw[32] = {
    0x68, 0xbf, 0x2a, 0x65, 0xe4, 0x24, 0x7d, 0xc3, 0xc7, 0x8a, 0x0f, 0x80, 0x2c, 0x52, 0x88, 0xc4,
    0x1f, 0xec, 0x85, 0x70, 0x3c, 0xbd, 0xcc, 0xb0, 0x27, 0x74, 0xff, 0x3b, 0x59, 0xef, 0x56, 0x4b,
};

const char_t* public_key_in_pem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VuAyEASk8Io9zqM0Ko2x/wJQEisvUn0bLt+JaLFapZ6Bi9xj0=\n"
    "-----END PUBLIC KEY-----\n";

const uint8_t public_key_raw[32] = {
    0x4a, 0x4f, 0x08, 0xa3, 0xdc, 0xea, 0x33, 0x42, 0xa8, 0xdb, 0x1f, 0xf0, 0x25, 0x01, 0x22, 0xb2,
    0xf5, 0x27, 0xd1, 0xb2, 0xed, 0xf8, 0x96, 0x8b, 0x15, 0xaa, 0x59, 0xe8, 0x18, 0xbd, 0xc6, 0x3d,
};


int main(void) {
    int8_t res;
    uint8_t private_key_raw_local[X25519_PRIVATE_KEY_RAW_LEN];
    res = pem_read_x25519_private_key(private_key_in_pem, private_key_raw_local);

    if (res != 0) {
        print_error("Failed to read private key from PEM\n");
        return -1;
    }

    // print the raw private key
    printf("Raw Private Key:\n");
    for (size_t i = 0; i < X25519_PRIVATE_KEY_RAW_LEN; i++) {
        printf("%02x", private_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(private_key_raw_local, private_key_raw, 32) != 0) {
        print_error("Private key does not match expected raw value\n");
        return -1;
    }

    print_success("Private key successfully read and verified\n");

    char_t* private_key_out_pem = NULL;
    res = pem_write_x25519_private_key(private_key_raw_local, &private_key_out_pem);

    if (res != 0) {
        print_error("Failed to write private key to PEM\n");
        return -1;
    }

    printf("Written Private Key PEM:\n%s\n", private_key_out_pem);

    if (memory_memcompare(private_key_out_pem, private_key_in_pem, strlen(private_key_in_pem)) != 0) {
        print_error("Written private key PEM does not match original\n");
        memory_free(private_key_out_pem);
        return -1;
    }

    print_success("Private key successfully written to PEM and verified\n");
    memory_free(private_key_out_pem);

    uint8_t public_key_raw_local[X25519_PUBLIC_KEY_RAW_LEN];
    res = pem_read_x25519_public_key(public_key_in_pem, public_key_raw_local);

    if (res != 0) {
        print_error("Failed to read public key from PEM\n");
        return -1;
    }

    // print the raw public key
    printf("Raw Public Key:\n");
    for (size_t i = 0; i < X25519_PUBLIC_KEY_RAW_LEN; i++) {
        printf("%02x", public_key_raw_local[i]);
        if (i % 16 == 15) {
            printf("\n");
        } else {
            printf(" ");
        }
    }
    printf("\n");

    if (memory_memcompare(public_key_raw_local, public_key_raw, 32) != 0) {
        print_error("Public key does not match expected raw value\n");
        return -1;
    }

    print_success("Public key successfully read and verified\n");

    char_t* public_key_out_pem = NULL;
    res = pem_write_x25519_public_key(public_key_raw_local, &public_key_out_pem);

    if (res != 0) {
        print_error("Failed to write public key to PEM\n");
        return -1;
    }

    printf("Written Public Key PEM:\n%s\n", public_key_out_pem);

    if (memory_memcompare(public_key_out_pem, public_key_in_pem, strlen(public_key_in_pem)) != 0) {
        print_error("Written public key PEM does not match original\n");
        memory_free(public_key_out_pem);
        return -1;
    }

    print_success("Public key successfully written to PEM and verified\n");
    memory_free(public_key_out_pem);

    uint8_t derived_public_key[X25519_PUBLIC_KEY_RAW_LEN];
    res = x25519_derive_public(derived_public_key, private_key_raw_local);

    if (res != 0) {
        print_error("Failed to derive public key from private key\n");
        return -1;
    }

    if (memory_memcompare(derived_public_key, public_key_raw, 32) != 0) {
        print_error("Derived public key does not match expected public key\n");
        return -1;
    }

    print_success("Public key successfully derived from private key and verified\n");

    uint8_t gen_private_key[X25519_PRIVATE_KEY_RAW_LEN];
    uint8_t gen_public_key[X25519_PUBLIC_KEY_RAW_LEN];
    res = x25519_generate_keypair(gen_private_key, gen_public_key);

    if (res != 0) {
        print_error("Failed to generate X25519 keypair\n");
        return -1;
    }

    print_success("X25519 keypair successfully generated\n");

    char_t* gen_private_key_pem = NULL;
    res = pem_write_x25519_private_key(gen_private_key, &gen_private_key_pem);

    if (res != 0) {
        print_error("Failed to write generated private key to PEM\n");
        return -1;
    }

    char_t* gen_public_key_pem = NULL;
    res = pem_write_x25519_public_key(gen_public_key, &gen_public_key_pem);

    if (res != 0) {
        print_error("Failed to write generated public key to PEM\n");
        memory_free(gen_private_key_pem);
        return -1;
    }

    printf("Generated Private Key PEM:\n%s\n", gen_private_key_pem);
    printf("Generated Public Key PEM:\n%s\n", gen_public_key_pem);

    uint8_t from_gen_private_key_pem[X25519_PRIVATE_KEY_RAW_LEN];
    res = pem_read_x25519_private_key(gen_private_key_pem, from_gen_private_key_pem);

    if (res != 0) {
        print_error("Failed to read generated private key from PEM\n");
        return -1;
    }

    if (memory_memcompare(from_gen_private_key_pem, gen_private_key, 32) != 0) {
        print_error("Generated private key read from PEM does not match original\n");
        return -1;
    }

    print_success("Generated private key successfully read from PEM and verified\n");

    uint8_t from_gen_public_key_pem[X25519_PUBLIC_KEY_RAW_LEN];
    res = pem_read_x25519_public_key(gen_public_key_pem, from_gen_public_key_pem);

    if (res != 0) {
        print_error("Failed to read generated public key from PEM\n");
        return -1;
    }

    if (memory_memcompare(from_gen_public_key_pem, gen_public_key, 32) != 0) {
        print_error("Generated public key read from PEM does not match original\n");
        return -1;
    }

    print_success("Generated public key successfully read from PEM and verified\n");

    memory_free(gen_private_key_pem);
    memory_free(gen_public_key_pem);


    uint8_t shared_secret1[X25519_SHARED_SECRET_LEN];
    uint8_t shared_secret2[X25519_SHARED_SECRET_LEN];
    // Party A computes shared secret
    res = x25519_shared_secret(shared_secret1, private_key_raw_local, gen_public_key);
    if (res != 0) {
        print_error("Party A failed to compute shared secret\n");
        return -1;
    }
    // Party B computes shared secret
    res = x25519_shared_secret(shared_secret2, gen_private_key, public_key_raw_local);
    if (res != 0) {
        print_error("Party B failed to compute shared secret\n");
        return -1;
    }

    if (memory_memcompare(shared_secret1, shared_secret2, X25519_SHARED_SECRET_LEN) != 0) {
        print_error("Shared secrets do not match\n");
        return -1;
    }

    print_success("Shared secret successfully computed and verified between both parties\n");



    return 0;
}
