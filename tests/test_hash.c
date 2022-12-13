/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <sha2.h>
#include <xxhash.h>
#include <strings.h>
#include <utils.h>

uint32_t main(uint32_t argc, char_t** argv) {
    setup_ram();

    if(argc != 2) {
        print_error("incorrect args");

        return -1;
    }

    argv++;

    sha256_ctx_t ctx1 = sha256_init();

    if(!ctx1) {
        print_error("cannot create sha256 ctx");

        return -1;
    }

    sha512_ctx_t ctx2 = sha512_init();

    if(!ctx2) {
        print_error("cannot create sha512 ctx");

        return -1;
    }

    sha224_ctx_t ctx3 = sha224_init();

    if(!ctx3) {
        print_error("cannot create sha224 ctx");

        return -1;
    }

    sha384_ctx_t ctx4 = sha384_init();

    if(!ctx4) {
        print_error("cannot create sha384 ctx");

        return -1;
    }

    FILE* fp_in = fopen(*argv, "r");

    if(!fp_in) {
        print_error("cannot open file");

        return -1;
    }

    uint8_t chunk[4096];

    while(1) {
        int64_t r = fread(chunk, 1, 4096, fp_in);

        if(!r) {
            break;
        }

        sha256_update(ctx1, chunk, r);
        sha512_update(ctx2, chunk, r);
        sha224_update(ctx3, chunk, r);
        sha384_update(ctx4, chunk, r);
    }

    fclose(fp_in);

    uint8_t* sum1 = sha256_final(ctx1);
    uint8_t* sum2 = sha512_final(ctx2);
    uint8_t* sum3 = sha224_final(ctx3);
    uint8_t* sum4 = sha384_final(ctx4);


    for(int64_t i = 0; i < SHA256_OUTPUT_SIZE; i++) {
        printf("%02x", sum1[i]);
    }
    printf(" %s\n", *argv);

    for(int64_t i = 0; i < SHA224_OUTPUT_SIZE; i++) {
        printf("%02x", sum3[i]);
    }
    printf(" %s\n", *argv);

    for(int64_t i = 0; i < SHA512_OUTPUT_SIZE; i++) {
        printf("%02x", sum2[i]);
    }
    printf(" %s\n", *argv);

    for(int64_t i = 0; i < SHA384_OUTPUT_SIZE; i++) {
        printf("%02x", sum4[i]);
    }
    printf(" %s\n", *argv);

    memory_free(sum1);
    memory_free(sum2);
    memory_free(sum3);
    memory_free(sum4);

    char_t* xxhash_data = "Hello, world!";
    uint64_t xxhash_result = xxhash64_hash(xxhash_data, strlen(xxhash_data)), xxhash_verify = 17691043854468224118ULL;
    printf("%lu %i\n", xxhash_result, xxhash_result == xxhash_verify);

    return 0;
}
