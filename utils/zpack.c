/**
 * @file zpack.c
 * @brief ZPACK compression tool on host.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <zpack.h>
#include <xxhash.h>
#include <strings.h>

int32_t main(int32_t argc, char_t** argv) {

    if(argc != 4) {
        print_error("parameter error");

        printf("Usage:\n\t%s <c|d> <infile> <outfile>\n\n", argv[0]);

        return -1;
    }

    FILE* fd = fopen(argv[2], "r");

    if (!fd) {
        return 1;
    }

    fseek(fd, 0, SEEK_END);

    int64_t in_size = ftell(fd);

    fseek(fd, 0, SEEK_SET);

    uint8_t* in_data = memory_malloc(in_size);

    if(in_data == NULL) {
        fclose(fd);

        return -1;
    }

    fread(in_data, 1, in_size, fd);
    fclose(fd);


    boolean_t comp = true;

    if(*argv[1] == 'c') {
        comp = true;
    } else if(*argv[1] == 'd') {
        comp = false;
    } else {
        print_error("parameter error");

        printf("Usage:\n\t%s <c|d> <infile> <outfile>\n\n", argv[0]);

        memory_free(in_data);

        return -1;
    }

    buffer_t* inbuf  = NULL;
    buffer_t* outbuf = NULL;
    int64_t ps = 0;

    uint64_t unpacked_hash = 0;
    uint64_t unpacked_size = 0;

    if(comp) {
        inbuf  = buffer_encapsulate(in_data, in_size);
    } else {
        compression_header_t* zf = (compression_header_t*)in_data;

        uint64_t packed_hash = xxhash64_hash(in_data + sizeof(compression_header_t), zf->packed_size);
        unpacked_hash = zf->unpacked_hash;
        unpacked_size = zf->unpacked_size;

        if(zf->packed_hash != packed_hash) {
            print_error("corrupted file");
            return -1;
        }

        inbuf  = buffer_encapsulate(in_data + sizeof(compression_header_t), zf->packed_size);
    }


    printf("input size %lli\n", buffer_get_length(inbuf));

    if(*argv[1] == 'c') {
        outbuf = buffer_new_with_capacity(NULL, in_size);
        if(zpack_pack(inbuf, outbuf) != 0) {
            print_error("pack failed");

            buffer_destroy(inbuf);
            memory_free(in_data);
            buffer_destroy(outbuf);

            return -1;
        }

        ps = buffer_get_length(outbuf);
    } else if(*argv[1] == 'd') {
        printf("expected unpacked size %lli\n", unpacked_size);

        outbuf = buffer_new_with_capacity(NULL, unpacked_size);

        if(!outbuf) {
            print_error("unpack failed, cannot create outbuf");

            buffer_destroy(inbuf);
            memory_free(in_data);

            return -1;
        }

        if(zpack_unpack(inbuf, outbuf) != 0) {
            print_error("unpack failed");

            buffer_destroy(inbuf);
            memory_free(in_data);
            buffer_destroy(outbuf);

            return -1;
        }

        ps = buffer_get_length(outbuf);

        if(unpacked_size != (uint64_t)ps ) {
            print_error("unpack failed, size mismatch");

            buffer_destroy(inbuf);
            memory_free(in_data);
            buffer_destroy(outbuf);

            return -1;
        }

        printf("unpacked size %lli\n", ps);
    }

    buffer_destroy(inbuf);

    uint8_t* outdata = buffer_get_all_bytes(outbuf, NULL);

    buffer_destroy(outbuf);

    if(comp) {
        fd = fopen(argv[3], "w");

        compression_header_t zf = {COMPRESSION_HEADER_MAGIC, COMPRESSION_TYPE_ZPACK, in_size, ps, xxhash64_hash(in_data, in_size), xxhash64_hash(outdata, ps)};

        memory_free(in_data);

        fwrite(&zf, 1, sizeof(zf), fd);
    } else {
        memory_free(in_data);

        if(unpacked_hash != xxhash64_hash(outdata, ps)) {
            print_error("unpack hash mismatch");
            memory_free(outdata);

            return -1;
        }

        fd = fopen(argv[3], "w");
    }

    fwrite(outdata, 1, ps, fd);

    memory_free(outdata);

    if(comp) {
        uint64_t pad_len = (((ps + sizeof(compression_header_t) + 511) / 512) * 512) - ps - sizeof(compression_header_t);

        if(pad_len) {
            printf("padding required with len %lli\n", pad_len);

            uint8_t* pad = memory_malloc(pad_len);

            fwrite(pad, 1, pad_len, fd);

            memory_free(pad);
        }
    }

    fclose(fd);

    return 0;
}
