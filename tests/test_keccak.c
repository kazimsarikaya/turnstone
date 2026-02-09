/**
 * @file test_keccak.c
 * @brief Keccak (SHA-3) implementation test
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <strings.h>
#include <crypto/keccak.h>

int32_t main(void);

static uint8_t sha3_224_test_result[] = {
    0xd1, 0x5d, 0xad, 0xce, 0xaa, 0x4d, 0x5d, 0x7b, 0xb3, 0xb4, 0x8f, 0x44,
    0x64, 0x21, 0xd5, 0x42, 0xe0, 0x8a, 0xd8, 0x88, 0x73, 0x05, 0xe2, 0x8d,
    0x58, 0x33, 0x57, 0x95,
};

static uint8_t sha3_256_test_result[] = {
    0x69, 0x07, 0x0d, 0xda, 0x01, 0x97, 0x5c, 0x8c, 0x12, 0x0c, 0x3a, 0xad,
    0xa1, 0xb2, 0x82, 0x39, 0x4e, 0x7f, 0x03, 0x2f, 0xa9, 0xcf, 0x32, 0xf4,
    0xcb, 0x22, 0x59, 0xa0, 0x89, 0x7d, 0xfc, 0x04,
};

static uint8_t sha3_384_test_result[] = {
    0x70, 0x63, 0x46, 0x5e, 0x08, 0xa9, 0x3b, 0xce, 0x31, 0xcd, 0x89, 0xd2,
    0xe3, 0xca, 0x8f, 0x60, 0x24, 0x98, 0x69, 0x6e, 0x25, 0x35, 0x92, 0xed,
    0x26, 0xf0, 0x7b, 0xf7, 0xe7, 0x03, 0xcf, 0x32, 0x85, 0x81, 0xe1, 0x47,
    0x1a, 0x7b, 0xa7, 0xab, 0x11, 0x9b, 0x1a, 0x9e, 0xbd, 0xf8, 0xbe, 0x41
};

static uint8_t sha3_512_test_result[] = {
    0x01, 0xde, 0xdd, 0x5d, 0xe4, 0xef, 0x14, 0x64, 0x24, 0x45, 0xba, 0x5f,
    0x5b, 0x97, 0xc1, 0x5e, 0x47, 0xb9, 0xad, 0x93, 0x13, 0x26, 0xe4, 0xb0,
    0x72, 0x7c, 0xd9, 0x4c, 0xef, 0xc4, 0x4f, 0xff, 0x23, 0xf0, 0x7b, 0xf5,
    0x43, 0x13, 0x99, 0x39, 0xb4, 0x91, 0x28, 0xca, 0xf4, 0x36, 0xdc, 0x1b,
    0xde, 0xe5, 0x4f, 0xcb, 0x24, 0x02, 0x3a, 0x08, 0xd9, 0x40, 0x3f, 0x9b,
    0x4b, 0xf0, 0xd4, 0x50,
};

static uint8_t shake128_test_result[] = {
    0xf4, 0x20, 0x2e, 0x3c, 0x58, 0x52, 0xf9, 0x18, 0x2a, 0x04, 0x30, 0xfd,
    0x81, 0x44, 0xf0, 0xa7, 0x4b, 0x95, 0xe7, 0x41, 0x7e, 0xca, 0xe1, 0x7d,
    0xb0, 0xf8, 0xcf, 0xee, 0xd0, 0xe3, 0xe6, 0x6e,
};

static uint8_t shake256_test_result[] = {
    0x2f, 0x67, 0x13, 0x43, 0xd9, 0xb2, 0xe1, 0x60, 0x4d, 0xc9, 0xdc, 0xf0,
    0x75, 0x3e, 0x5f, 0xe1, 0x5c, 0x7c, 0x64, 0xa0, 0xd2, 0x83, 0xcb, 0xbf,
    0x72, 0x2d, 0x41, 0x1a, 0x0e, 0x36, 0xf6, 0xca, 0x1d, 0x01, 0xd1, 0x36,
    0x9a, 0x23, 0x53, 0x9c, 0xd8, 0x0f, 0x7c, 0x05, 0x4b, 0x6e, 0x5d, 0xaf,
    0x9c, 0x96, 0x2c, 0xad, 0x5b, 0x8e, 0xd5, 0xbd, 0x11, 0x99, 0x8b, 0x40,
    0xd5, 0x73, 0x44, 0x42,
};

int32_t main(void) {
    uint8_t* hash;
    const char* test_str = "The quick brown fox jumps over the lazy dog";

    hash = sha3_224_hash((const uint8_t*)test_str, strlen(test_str));
    if(hash) {
        printf("SHA3-224: ");
        for(size_t i = 0; i < 224 / 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, sha3_224_test_result, SHA3_224_OUTPUT_SIZE) == 0) {
            print_success("SHA3-224 test passed");
        } else {
            print_error("SHA3-224 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHA3-224 failed");
    }

    hash = sha3_256_hash((const uint8_t*)test_str, strlen(test_str));
    if(hash) {
        printf("SHA3-256: ");
        for(size_t i = 0; i < 256 / 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, sha3_256_test_result, SHA3_256_OUTPUT_SIZE) == 0) {
            print_success("SHA3-256 test passed");
        } else {
            print_error("SHA3-256 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHA3-256 failed");
    }

    hash = sha3_384_hash((const uint8_t*)test_str, strlen(test_str));
    if(hash) {
        printf("SHA3-384: ");
        for(size_t i = 0; i < 384 / 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, sha3_384_test_result, SHA3_384_OUTPUT_SIZE) == 0) {
            print_success("SHA3-384 test passed");
        } else {
            print_error("SHA3-384 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHA3-384 failed");
    }

    hash = sha3_512_hash((const uint8_t*)test_str, strlen(test_str));
    if(hash) {
        printf("SHA3-512: ");
        for(size_t i = 0; i < 512 / 8; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, sha3_512_test_result, SHA3_512_OUTPUT_SIZE) == 0) {
            print_success("SHA3-512 test passed");
        } else {
            print_error("SHA3-512 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHA3-512 failed");
    }

    hash = shake128_hash((const uint8_t*)test_str, strlen(test_str), 32);
    if(hash) {
        printf("SHAKE128 (32 bytes): ");
        for(size_t i = 0; i < 32; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, shake128_test_result, 32) == 0) {
            print_success("SHAKE128 test passed");
        } else {
            print_error("SHAKE128 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHAKE128 failed");
    }

    hash = shake256_hash((const uint8_t*)test_str, strlen(test_str), 64);
    if(hash) {
        printf("SHAKE256 (64 bytes): ");
        for(size_t i = 0; i < 64; i++) {
            printf("%02x", hash[i]);
        }
        printf("\n");

        if(memory_memcompare(hash, shake256_test_result, 64) == 0) {
            print_success("SHAKE256 test passed");
        } else {
            print_error("SHAKE256 test failed");
        }

        memory_free(hash);
    } else {
        print_error("SHAKE256 failed");
    }

    return 0;
}
