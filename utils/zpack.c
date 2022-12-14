/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x8000000
#include "setup.h"
#include <zpack.h>
#include <xxhash.h>

int32_t main(int32_t argc, char_t** argv) {

    UNUSED(argc);

    FILE* fd = fopen(argv[2], "r");

    if (!fd) {
        return 1;
    }

    fseek(fd, 0, SEEK_END);

    int64_t in_size = ftell(fd);

    fseek(fd, 0, SEEK_SET);

    uint8_t in_data[in_size];

    fread(in_data, 1, in_size, fd);
    fclose(fd);

    buffer_t inbuf = buffer_encapsulate(in_data, in_size);
    buffer_t outbuf = NULL;
    int64_t ps = 0;

    printf("input size %lli\n", buffer_get_length(inbuf));

    if(*argv[1] == 'c') {
        outbuf = buffer_new_with_capacity(NULL, in_size);
        ps = zpack_pack(inbuf, outbuf);
        printf("packed size %lli\n", ps);
    } else if(*argv[1] == 'd') {
        outbuf = buffer_new_with_capacity(NULL, in_size * 2);
        ps = zpack_unpack(inbuf, outbuf);
        printf("unpacked size %lli\n", ps);
    } else {
        return -1;
    }

    buffer_destroy(inbuf);


    fd = fopen(argv[3], "w");

    uint8_t* outdata = buffer_get_all_bytes(outbuf, NULL);

    buffer_destroy(outbuf);

    zpack_format_t zf = {ZPACK_FORMAT_MAGIC, in_size, ps, xxhash64_hash(in_data, in_size), xxhash64_hash(outdata, ps)};

    fwrite(&zf, 1, sizeof(zf), fd);

    fwrite(outdata, 1, ps, fd);

    memory_free(outdata);

    uint64_t pad_len = (((ps + sizeof(zf) + 511) / 512) * 512) - ps - sizeof(zf);

    if(pad_len) {
        printf("padding required with len %i\n", pad_len);

        uint8_t* pad = memory_malloc(pad_len);

        fwrite(pad, 1, pad_len, fd);

        memory_free(pad);
    }

    fclose(fd);

    return 0;
}
