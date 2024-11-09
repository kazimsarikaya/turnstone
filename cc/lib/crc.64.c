/**
 * @file crc.64.c
 * @brief CRC32 implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crc.h>

MODULE("turnstone.lib");


uint32_t crc32_table[256] = {};

void crc32_init_table(void) {
    uint8_t index = 0, z;
    do{
        crc32_table[index] = index;
        for(z = 8; z; z--) {
            crc32_table[index] = (crc32_table[index] & 1)?((crc32_table[index] >> 1) ^ 0xEDB88320):(crc32_table[index] >> 1);
        }
    }while(++index);
}

uint32_t crc32_sum(const void* p, uint32_t bytelength, uint32_t init) {
    uint8_t* p_u8 = (uint8_t*) p;
    uint32_t crc = init;
    while (bytelength-- != 0) {
        crc = crc32_table[((uint8_t) crc ^ *(p_u8++))] ^ (crc >> 8);
    }
    return crc;
}

static inline uint32_t crc32c_u8(uint32_t crc, uint8_t data) {
    uint32_t ret = 0;
    asm volatile (
        "crc32b %1, %0"
        : "=r" (ret)
        : "r" (data), "0" (crc)
        );
    return ret;
}

static inline uint32_t crc32c_u32(uint32_t crc, uint32_t data) {
    uint32_t ret = 0;
    asm volatile (
        "crc32l %1, %0"
        : "=r" (ret)
        : "r" (data), "0" (crc)
        );
    return ret;
}

uint32_t crc32c_sum(const void* data, uint64_t size, uint32_t init) {
    uint32_t ret = init;
    const uint32_t* data32 = (const uint32_t*)data;

    while(size >= 4) {
        ret = crc32c_u32(ret, *data32);
        data32++;
        size -= 4;
    }

    const uint8_t* data8 = (const uint8_t*)data32;

    while(size > 0) {
        ret = crc32c_u8(ret, *data8);
        data8++;
        size--;
    }

    return ret;
}

uint32_t adler32_sum(const void* data, uint64_t size, uint32_t init) {
    uint32_t a = init & 0xFFFF;
    uint32_t b = (init >> 16) & 0xFFFF;

    const uint8_t* data8 = (const uint8_t*)data;

    while(size > 0) {
        uint32_t len = size > 5550 ? 5550 : size;
        size -= len;

        do {
            a += *data8++;
            b += a;
        } while(--len);

        a %= 65521;
        b %= 65521;
    }

    return (b << 16) | a;
}
