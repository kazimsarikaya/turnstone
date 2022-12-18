/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___ZPACK_H
#define ___ZPACK_H 0

#include <buffer.h>

#define ZPACK_FORMAT_MAGIC 0x544D464B4341505AULL

typedef struct {
    uint64_t magic;
    uint64_t unpacked_size;
    uint64_t packed_size;
    uint64_t unpacked_hash;
    uint64_t packed_hash;
} __attribute__((packed)) zpack_format_t;

int64_t zpack_pack(buffer_t in, buffer_t out);
int64_t zpack_unpack(buffer_t in, buffer_t out);

#endif
