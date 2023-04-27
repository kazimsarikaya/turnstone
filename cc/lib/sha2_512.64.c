/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <sha2.h>
#include <memory.h>
#include <utils.h>

MODULE("turnstone.lib");


#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT64(x, 28) ^ ROTRIGHT64(x, 34) ^ ROTRIGHT64(x, 39))
#define EP1(x) (ROTRIGHT64(x, 14) ^ ROTRIGHT64(x, 18) ^ ROTRIGHT64(x, 41))
#define SIG0(x) (ROTRIGHT64(x, 1) ^ ROTRIGHT64(x, 8) ^ ((x) >> 7))
#define SIG1(x) (ROTRIGHT64(x, 19) ^ ROTRIGHT64(x, 61) ^ ((x) >> 6))


typedef struct sha512_internal_ctx_t {
    uint8_t  data[SHA512_BLOCK_SIZE];
    uint32_t datalen;
    uint64_t bitlen;
    uint64_t state[SHA512_STATE_SIZE];
} sha512_internal_ctx_t;

void sha512_transform(sha512_internal_ctx_t* ctx, const uint8_t* data);

static const uint64_t k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
    0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
    0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
    0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
    0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
    0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

uint8_t* sha512_hash(uint8_t* data, size_t length) {
    sha512_ctx_t ctx = sha512_init();
    sha512_update(ctx, data, length);
    return sha512_final(ctx);
}

void sha512_transform(sha512_internal_ctx_t* ctx, const uint8_t* data)
{
    uint64_t a, b, c, d, e, f, g, h, i, t1, t2, m[SHA512_BLOCK_SIZE];

    uint64_t* t_data = (uint64_t*)data;

    for (i = 0; i < 16; ++i) {
        m[i] = BYTE_SWAP64(t_data[i]);
    }

    for ( ; i < 80; ++i) {
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

    for (i = 0; i < 80; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
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



sha512_ctx_t sha512_init(void) {
    sha512_internal_ctx_t* ctx = memory_malloc(sizeof(sha512_internal_ctx_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;

    return (sha512_ctx_t)ctx;
}

int8_t sha512_update(sha512_ctx_t ctx, const uint8_t* data, size_t len) {

    if(ctx == NULL) {
        return -1;
    }

    sha512_internal_ctx_t* ictx = (sha512_internal_ctx_t*)ctx;

    for (uint32_t i = 0; i < len; ++i) {
        ictx->data[ictx->datalen] = data[i];
        ictx->datalen++;

        if (ictx->datalen == SHA512_BLOCK_SIZE) {
            sha512_transform(ictx, ictx->data);

            ictx->bitlen += SHA512_BLOCK_SIZE * 8;
            ictx->datalen = 0;
        }
    }

    return 0;
}

uint8_t* sha512_final(sha512_ctx_t ctx) {

    if(ctx == NULL) {
        return NULL;
    }

    sha512_internal_ctx_t* ictx = (sha512_internal_ctx_t*)ctx;
    uint32_t i;

    i = ictx->datalen;

    if (ictx->datalen < 120) {
        ictx->data[i++] = 0x80;

        while (i < 120) {
            ictx->data[i++] = 0x00;
        }
    }
    else {
        ictx->data[i++] = 0x80;

        while (i < SHA256_BLOCK_SIZE) {
            ictx->data[i++] = 0x00;
        }

        sha512_transform(ictx, ictx->data);
        memory_memset(ictx->data, 0, SHA256_BLOCK_SIZE);
    }

    ictx->bitlen += ictx->datalen * 8;
    ictx->data[127] = ictx->bitlen;
    ictx->data[126] = ictx->bitlen >> 8;
    ictx->data[125] = ictx->bitlen >> 16;
    ictx->data[124] = ictx->bitlen >> 24;
    ictx->data[123] = ictx->bitlen >> 32;
    ictx->data[122] = ictx->bitlen >> 40;
    ictx->data[121] = ictx->bitlen >> 48;
    ictx->data[120] = ictx->bitlen >> 56;

    sha512_transform(ictx, ictx->data);

    uint8_t* hash = memory_malloc(SHA512_OUTPUT_SIZE);

    if(hash == NULL) {
        memory_free(ictx);

        return NULL;
    }

    uint64_t* t_hash = (uint64_t*)hash;

    for (i = 0; i < 8; ++i) {
        t_hash[i] = BYTE_SWAP64(ictx->state[i]);
    }

    memory_free(ictx);

    return hash;
}

sha384_ctx_t sha384_init(void) {
    sha512_internal_ctx_t* ctx = memory_malloc(sizeof(sha512_internal_ctx_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0xcbbb9d5dc1059ed8ULL;
    ctx->state[1] = 0x629a292a367cd507ULL;
    ctx->state[2] = 0x9159015a3070dd17ULL;
    ctx->state[3] = 0x152fecd8f70e5939ULL;
    ctx->state[4] = 0x67332667ffc00b31ULL;
    ctx->state[5] = 0x8eb44a8768581511ULL;
    ctx->state[6] = 0xdb0c2e0d64f98fa7ULL;
    ctx->state[7] = 0x47b5481dbefa4fa4ULL;

    return (sha384_ctx_t)ctx;
}

int8_t  sha384_update(sha384_ctx_t ctx, const uint8_t* data, size_t len) {
    return sha512_update((sha512_ctx_t)ctx, data, len);
}

uint8_t* sha384_final(sha384_ctx_t ctx) {
    uint8_t* pre_hash = sha512_final((sha512_ctx_t)ctx);

    if(pre_hash == NULL) {
        return NULL;
    }

    uint8_t* hash = memory_malloc(SHA384_OUTPUT_SIZE);

    if(hash == NULL) {
        memory_free(pre_hash);

        return NULL;
    }

    memory_memcopy(pre_hash, hash, SHA384_OUTPUT_SIZE);

    memory_free(pre_hash);

    return hash;
}

uint8_t* sha384_hash(uint8_t* data, size_t length) {
    sha384_ctx_t ctx = sha384_init();
    sha384_update(ctx, data, length);
    return sha384_final(ctx);
}
