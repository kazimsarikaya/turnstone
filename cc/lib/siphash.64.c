/**
 * @file siphash.64.c
 * @brief sip hash interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <siphash.h>
#include <utils.h>

#define SIPHASH128_0 0x736f6d6570736575ULL
#define SIPHASH128_1 0x646f72616e646f6dULL
#define SIPHASH128_2 0x6c7967656e657261ULL
#define SIPHASH128_3 0x7465646279746573ULL


#define SIPROUND {                    \
            v0 += v1;                 \
            v1 = ROTLEFT64(v1, 13);   \
            v1 ^= v0;                 \
            v0 = ROTLEFT64(v0, 32);   \
            v2 += v3;                 \
            v3 = ROTLEFT64(v3, 16);   \
            v3 ^= v2;                 \
            v0 += v3;                 \
            v3 = ROTLEFT64(v3, 21);   \
            v3 ^= v0;                 \
            v2 += v1;                 \
            v1 = ROTLEFT64(v1, 17);   \
            v1 ^= v2;                 \
            v2 = ROTLEFT64(v2, 32);   \
}

uint128_t siphash128_internal(const void* data, uint64_t len, boolean_t full, uint128_t seed);

uint128_t siphash128_internal(const void* data, uint64_t len, boolean_t full, uint128_t seed) {

    const uint8_t* ni = (const uint8_t*)data;

    uint64_t v0 = SIPHASH128_0;
    uint64_t v1 = SIPHASH128_1;
    uint64_t v2 = SIPHASH128_2;
    uint64_t v3 = SIPHASH128_3;

    uint64_t k0 = seed;
    uint64_t k1 = seed >> 64;
    uint64_t m;

    int64_t i;

    const uint8_t* end = ni + len - (len % sizeof(uint64_t));
    const int64_t left = len & 7;
    uint64_t b = ((uint64_t)len) << 56;

    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    if(full) {
        v1 ^= 0xee;
    }

    for (; ni != end; ni += 8) {
        m = *((uint64_t*)(ni));
        v3 ^= m;

        for (i = 0; i < 2; ++i) {
            SIPROUND;
        }

        v0 ^= m;
    }

    switch (left) {
    case 7:
        b |= ((uint64_t)ni[6]) << 48;
        nobreak;
    case 6:
        b |= ((uint64_t)ni[5]) << 40;
        nobreak;
    case 5:
        b |= ((uint64_t)ni[4]) << 32;
        nobreak;
    case 4:
        b |= ((uint64_t)ni[3]) << 24;
        nobreak;
    case 3:
        b |= ((uint64_t)ni[2]) << 16;
        nobreak;
    case 2:
        b |= ((uint64_t)ni[1]) << 8;
        nobreak;
    case 1:
        b |= ((uint64_t)ni[0]);
        break;
    case 0:
        break;
    }

    v3 ^= b;

    for (i = 0; i < 2; ++i)
        SIPROUND;

    v0 ^= b;

    if(full) {
        v2 ^= 0xee;
    } else {
        v2 ^= 0xff;
    }

    for (i = 0; i < 4; ++i) {
        SIPROUND;
    }

    uint128_t h1;
    uint128_t h2;

    b = v0 ^ v1 ^ v2 ^ v3;
    h1 = b;

    if(!full) {
        return h1;
    }

    v1 ^= 0xdd;

    for (i = 0; i < 4; ++i) {
        SIPROUND;
    }

    b = v0 ^ v1 ^ v2 ^ v3;
    h2 = b;

    return h2 << 64 | h1;
}

uint128_t siphash128(const void* data, uint64_t len, uint128_t seed) {
    return siphash128_internal(data, len, true, seed);
}

uint64_t siphash64(const void* data, uint64_t len, uint64_t seed) {
    return siphash128_internal(data, len, false, seed);
}
