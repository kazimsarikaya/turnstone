/**
 * @file murmurhash.64.c
 * @brief murmur hash interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <murmurhash.h>
#include <utils.h>

MODULE("turnstone.lib");

#define MURMURHASH_64A_CONST 0xc6a4a7935bd1e995ULL
#define MURMURHASH_64A_R     47

uint64_t murmurhash64a(const void* data, uint64_t len, uint64_t seed) {
    uint64_t h = seed ^ (len * MURMURHASH_64A_CONST);

    const uint64_t* tmp_data = (const uint64_t*)data;
    const uint64_t* end = (len >> 3) + tmp_data;

    while(tmp_data != end)
    {
        uint64_t k = *tmp_data++;

        k *= MURMURHASH_64A_CONST;
        k ^= k >> MURMURHASH_64A_R;
        k *= MURMURHASH_64A_CONST;

        h ^= k;
        h *= MURMURHASH_64A_CONST;
    }

    const uint8_t* tmp_data2 = (const uint8_t*)tmp_data;

    switch(len & 7) {
    case 7:
        h ^= (uint64_t)(tmp_data2[6]) << 48;
        nobreak;
    case 6:
        h ^= (uint64_t)(tmp_data2[5]) << 40;
        nobreak;
    case 5:
        h ^= (uint64_t)(tmp_data2[4]) << 32;
        nobreak;
    case 4:
        h ^= (uint64_t)(tmp_data2[3]) << 24;
        nobreak;
    case 3:
        h ^= (uint64_t)(tmp_data2[2]) << 16;
        nobreak;
    case 2:
        h ^= (uint64_t)(tmp_data2[1]) << 8;
        nobreak;
    case 1:
        h ^= (uint64_t)(tmp_data2[0]);
        h *= MURMURHASH_64A_CONST;
        nobreak;
    default:
        break;
    }

    h ^= h >> MURMURHASH_64A_R;
    h *= MURMURHASH_64A_CONST;
    h ^= h >> MURMURHASH_64A_R;

    return h;
}

#define MURMURHASH3_128_1 0x87c37b91114253d5ULL
#define MURMURHASH3_128_2 0x4cf5ad432745937fULL
#define MURMURHASH3_128_3 0x52dce729
#define MURMURHASH3_128_4 0x38495ab5
#define MURMURHASH3_128_5 0xff51afd7ed558ccdULL
#define MURMURHASH3_128_6 0xc4ceb9fe1a85ec53ULL


static inline uint64_t murmurhash3_fmix64(uint64_t k) {
    k ^= k >> 33;
    k *= MURMURHASH3_128_5;
    k ^= k >> 33;
    k *= MURMURHASH3_128_6;
    k ^= k >> 33;

    return k;
}

uint128_t murmurhash3_128(const void* data, uint64_t len, uint64_t seed) {
    const uint8_t* tmp_data = (const uint8_t*)data;
    const int64_t nblocks = len / 16;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    const uint64_t* blocks = (const uint64_t*)(tmp_data);

    for(int64_t i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i * 2];
        uint64_t k2 = blocks[i * 2 + 1];

        k1 *= MURMURHASH3_128_1;
        k1  = ROTLEFT64(k1, 31);
        k1 *= MURMURHASH3_128_2;
        h1 ^= k1;

        h1 = ROTLEFT64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + MURMURHASH3_128_3;

        k2 *= MURMURHASH3_128_2;
        k2  = ROTLEFT64(k2, 33);
        k2 *= MURMURHASH3_128_1;
        h2 ^= k2;

        h2 = ROTLEFT64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + MURMURHASH3_128_4;
    }

    // ----------
    // tail

    const uint8_t* tail = (const uint8_t*)(tmp_data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch(len & 15) {
    case 15:
        k2 ^= ((uint64_t)tail[14]) << 48;
        nobreak;
    case 14:
        k2 ^= ((uint64_t)tail[13]) << 40;
        nobreak;
    case 13:
        k2 ^= ((uint64_t)tail[12]) << 32;
        nobreak;
    case 12:
        k2 ^= ((uint64_t)tail[11]) << 24;
        nobreak;
    case 11:
        k2 ^= ((uint64_t)tail[10]) << 16;
        nobreak;
    case 10:
        k2 ^= ((uint64_t)tail[ 9]) << 8;
        nobreak;
    case  9:
        k2 ^= ((uint64_t)tail[ 8]) << 0;
        k2 *= MURMURHASH3_128_2;
        k2  = ROTLEFT64(k2, 33);
        k2 *= MURMURHASH3_128_1;
        h2 ^= k2;
        nobreak;
    case  8:
        k1 ^= ((uint64_t)tail[ 7]) << 56;
        nobreak;
    case  7:
        k1 ^= ((uint64_t)tail[ 6]) << 48;
        nobreak;
    case  6:
        k1 ^= ((uint64_t)tail[ 5]) << 40;
        nobreak;
    case  5:
        k1 ^= ((uint64_t)tail[ 4]) << 32;
        nobreak;
    case  4:
        k1 ^= ((uint64_t)tail[ 3]) << 24;
        nobreak;
    case  3:
        k1 ^= ((uint64_t)tail[ 2]) << 16;
        nobreak;
    case  2:
        k1 ^= ((uint64_t)tail[ 1]) << 8;
        nobreak;
    case  1:
        k1 ^= ((uint64_t)tail[ 0]) << 0;
        k1 *= MURMURHASH3_128_1;
        k1  = ROTLEFT64(k1, 31);
        k1 *= MURMURHASH3_128_2;
        h1 ^= k1;
        nobreak;
    default:
        break;
    }

    // ----------
    // finalization

    h1 ^= len; h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = murmurhash3_fmix64(h1);
    h2 = murmurhash3_fmix64(h2);

    h1 += h2;
    h2 += h1;

    uint128_t res = 0;
    uint64_t* out = (uint64_t*)&res;
    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;

    return res;
}

