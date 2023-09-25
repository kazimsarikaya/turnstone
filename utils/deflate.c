/**
 * @file deflate.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#define RAMSIZE 0x8000000
#include "setup.h"
#include <compression.h>
#include <deflate.h>
#include <zpack.h>
#include <buffer.h>
#include <utils.h>
#include <strings.h>
#include <quicksort.h>

int32_t main(int32_t argc, char_t** argv);

int32_t main(int32_t argc, char_t** argv) {
    argc--;
    argv++;

    if (argc < 3) {
        printf("Usage: deflate <c|d> <input file> <output file>\n");
        return 1;
    }

    boolean_t compress = argv[0][0] == 'c';

    char_t* input = argv[1];
    char_t* output = argv[2];

    FILE* in = fopen(input, "rb");

    if (in == NULL) {
        print_error("Could not open input file %s\n", input);

        return 1;
    }

    fseek(in, 0, SEEK_END);
    uint64_t in_size = ftell(in);

    fseek(in, 0, SEEK_SET);

    uint8_t* in_data = memory_malloc(in_size);

    fread(in_data, 1, in_size, in);

    fclose(in);

    buffer_t* in_buf = buffer_encapsulate(in_data, in_size);

    buffer_t* out_buf = buffer_new_with_capacity(NULL, in_size * (compress ? 2 : 4));

    int8_t ret = 0;

    compression_t* compression = compression_get(COMPRESSION_TYPE_DEFLATE);

    if(compress) {
        ret = compression->pack(in_buf, out_buf);
    } else {
        ret = compression->unpack(in_buf, out_buf);
    }

    buffer_destroy(in_buf);
    memory_free(in_data);

    if (ret != 0) {
        buffer_destroy(out_buf);
        print_error("Compression failed with error code %d\n", ret);

        return ret;
    }

    FILE* out = fopen(output, "wb");

    uint64_t out_size = 0;
    uint8_t* out_data = buffer_get_all_bytes_and_destroy(out_buf, &out_size);

    fwrite(out_data, 1, out_size, out);

    memory_free(out_data);

    fclose(out);

    return 0;
}
