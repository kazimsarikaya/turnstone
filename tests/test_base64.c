/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <base64.h>
#include <strings.h>
#include <utils.h>

uint32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    uint8_t* original_data = (uint8_t*)"hello world";
    uint8_t* ref_data = (uint8_t*)"aGVsbG8gd29ybGQ=";

    uint8_t* enc_result = NULL;
    uint8_t* dec_result = NULL;

    size_t res_enc_len = 0, res_dec_len;

    res_enc_len = base64_encode(original_data, strlen((char_t*)original_data), true, &enc_result);

    if(res_enc_len == 0 || enc_result == NULL) {
        print_error("base64 encode failed");

        return -1;
    }

    if(strcmp((char_t*)enc_result, (char_t*)ref_data) != 0) {
        print_error("encode result wrong");

        memory_free(enc_result);

        return -1;
    }

    res_dec_len = base64_decode(enc_result, res_enc_len, &dec_result);

    memory_free(enc_result);

    if(res_dec_len == 0 || dec_result == NULL) {
        print_error("base64 decode failed");

        return -1;
    }

    if(strcmp((char_t*)original_data, (char_t*)dec_result) != 0) {
        print_error("in out mismatch");

        memory_free(dec_result);

        return -1;
    }

    memory_free(dec_result);

    print_success("TESTS PASSED");

    return 0;
}
