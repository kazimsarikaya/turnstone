/**
 * @file sha2_256.64.c
 * @brief SHA-256 implementation based on FIPS 180-4 (Federal Information Processing Standards Publication 180-4).
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <sha2.h>
#include <memory.h>
#include <utils.h>

MODULE("turnstone.lib.crypto");


#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT32(x, 2) ^ ROTRIGHT32(x, 13) ^ ROTRIGHT32(x, 22))
#define EP1(x) (ROTRIGHT32(x, 6) ^ ROTRIGHT32(x, 11) ^ ROTRIGHT32(x, 25))
#define SIG0(x) (ROTRIGHT32(x, 7) ^ ROTRIGHT32(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT32(x, 17) ^ ROTRIGHT32(x, 19) ^ ((x) >> 10))


typedef struct sha256_ctx_t {
    uint8_t  data[SHA256_BLOCK_SIZE];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[SHA256_STATE_SIZE];
} sha256_ctx_t;

void sha256_transform(sha256_ctx_t* ctx, const uint8_t* data);

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

uint8_t* sha256_hash(uint8_t* data, size_t length) {
    sha256_ctx_t* ctx = sha256_init();
    sha256_update(ctx, data, length);
    return sha256_final(ctx);
}

void sha256_transform(sha256_ctx_t* ctx, const uint8_t* data)
{
    uint32_t a, b, c, d, e, f, g, h, i, t1, t2, m[SHA256_BLOCK_SIZE];

    uint32_t* t_data = (uint32_t*)data;

    for (i = 0; i < 16; ++i) {
        m[i] = BYTE_SWAP32(t_data[i]);
    }

    for ( ; i < 64; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }


    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

sha256_ctx_t* sha256_init(void) {
    sha256_ctx_t* ctx = memory_malloc(sizeof(sha256_ctx_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    return ctx;
}

int8_t sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    if(ctx == NULL) {
        return -1;
    }

    for (uint32_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;

        if (ctx->datalen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->data);

            ctx->bitlen += SHA256_BLOCK_SIZE * 8;
            ctx->datalen = 0;
        }
    }

    return 0;
}

uint8_t* sha256_final(sha256_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }

    uint32_t i;

    i = ctx->datalen;

    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;

        while (i < 56) {
            ctx->data[i++] = 0x00;
        }
    }
    else {
        ctx->data[i++] = 0x80;

        while (i < SHA256_BLOCK_SIZE) {
            ctx->data[i++] = 0x00;
        }

        sha256_transform(ctx, ctx->data);
        memory_memset(ctx->data, 0, SHA256_BLOCK_SIZE);
    }

    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;

    sha256_transform(ctx, ctx->data);

    uint8_t* hash = memory_malloc(SHA256_OUTPUT_SIZE);

    if(hash == NULL) {
        memory_free(ctx);

        return NULL;
    }

    uint32_t* t_hash = (uint32_t*)hash;

    for (i = 0; i < 8; ++i) {
        t_hash[i] = BYTE_SWAP32(ctx->state[i]);
    }

    memory_free(ctx);

    return hash;
}

sha224_ctx_t* sha224_init(void) {
    sha256_ctx_t* ctx = memory_malloc(sizeof(sha256_ctx_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0xc1059ed8;
    ctx->state[1] = 0x367cd507;
    ctx->state[2] = 0x3070dd17;
    ctx->state[3] = 0xf70e5939;
    ctx->state[4] = 0xffc00b31;
    ctx->state[5] = 0x68581511;
    ctx->state[6] = 0x64f98fa7;
    ctx->state[7] = 0xbefa4fa4;

    return ctx;
}

int8_t  sha224_update(sha224_ctx_t* ctx, const uint8_t* data, size_t len) {
    return sha256_update(ctx, data, len);
}

uint8_t* sha224_final(sha224_ctx_t* ctx) {

    if(ctx == NULL) {
        return NULL;
    }

    uint8_t* pre_hash = sha256_final(ctx);

    if(pre_hash == NULL) {
        return NULL;
    }

    uint8_t* hash = memory_malloc(SHA224_OUTPUT_SIZE);

    if(hash == NULL) {
        memory_free(pre_hash);

        return NULL;
    }

    memory_memcopy(pre_hash, hash, SHA224_OUTPUT_SIZE);

    memory_free(pre_hash);

    return hash;
}

uint8_t* sha224_hash(uint8_t* data, size_t length) {
    sha224_ctx_t* ctx = sha224_init();
    sha224_update(ctx, data, length);
    return sha224_final(ctx);
}
