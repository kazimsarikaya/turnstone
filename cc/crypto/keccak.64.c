/**
 * @file keccak.64.c
 * @brief Keccak (SHA-3) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <crypto/keccak.h>
#include <memory.h>

MODULE("turnstone.lib.crypto");

typedef struct keccak_ctx_t keccak_ctx_t;

typedef enum keccak_variant_t {
    KECCAK_VARIANT_SHA3_224,
    KECCAK_VARIANT_SHA3_256,
    KECCAK_VARIANT_SHA3_384,
    KECCAK_VARIANT_SHA3_512,
    KECCAK_VARIANT_SHAKE128,
    KECCAK_VARIANT_SHAKE256,
} keccak_variant_t;

struct keccak_ctx_t {
    keccak_variant_t variant;
    uint64_t         state[25]   __attribute__((aligned(16)));
    uint8_t          buffer[200] __attribute__((aligned(16)));
    size_t           rate;
    size_t           capacity;
    size_t           output_len;
    size_t           buffer_len;
};


static keccak_ctx_t* keccak_init(keccak_variant_t variant, size_t rate, size_t capacity, size_t output_len) {
    keccak_ctx_t* ctx = memory_malloc(sizeof(keccak_ctx_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->variant = variant;
    ctx->rate = rate;
    ctx->capacity = capacity;
    ctx->output_len = output_len;
    ctx->buffer_len = 0;

    return ctx;
}

#define KECCAK_ROL64(a, n) (((a) << (n)) | ((a) >> (64 - (n))))

__attribute__((aligned(16)))
static const uint64_t keccak_f1600_constants[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

static void keccak_theta_step(keccak_ctx_t* ctx) {
    __attribute__((aligned(16))) uint64_t C[5] = {0}, D[5] = {0};

    // Compute C[x] = A[x,0] XOR A[x,1] XOR A[x,2] XOR A[x,3] XOR A[x,4]
    for (int32_t x = 0; x < 5; x++) {
        C[x] = ctx->state[x] ^ ctx->state[x + 5] ^ ctx->state[x + 10] ^ ctx->state[x + 15] ^ ctx->state[x + 20];
    }

    // Compute D[x] = C[(x-1) mod 5] XOR ROTL(C[(x+1) mod 5], 1)
    for (int32_t x = 0; x < 5; x++) {
        D[x] = C[(x + 4) % 5] ^ KECCAK_ROL64(C[(x + 1) % 5], 1);

        for (int32_t y = 0; y < 5; y++) {
            ctx->state[y * 5 + x] ^= D[x];
        }
    }
}

__attribute__((aligned(16)))
static const int32_t keccak_rho_offsets[25] = {
    0, 1, 62, 28, 27,
    36, 44, 6, 55, 20,
    3, 10, 43, 25, 39,
    41, 45, 15, 21, 8,
    18, 2, 61, 56, 14,
};

static void keccak_rho_step(keccak_ctx_t* ctx) {
    for (int32_t i = 0; i < 25; i++) {
        ctx->state[i] = KECCAK_ROL64(ctx->state[i], keccak_rho_offsets[i]);
    }
}

static void keccak_pi_step(keccak_ctx_t* ctx) {
    __attribute__((aligned(16))) uint64_t temp[25] = {0};
    for (int32_t x = 0; x < 5; x++) {
        for (int32_t y = 0; y < 5; y++) {
            // Mapping: (x, y) -> (y, (2*x + 3*y) % 5)
            temp[((2 * x + 3 * y) % 5) * 5 + y] = ctx->state[y * 5 + x];
        }
    }
    for (int32_t i = 0; i < 25; i++) {ctx->state[i] = temp[i];}
}

static void keccak_chi_step(keccak_ctx_t* ctx) {
    uint64_t temp[25] = {0};
    for (int32_t y = 0; y < 5; y++) {
        for (int32_t x = 0; x < 5; x++) {
            // A[x,y] = B[x,y] ^ ((~B[x+1,y]) & B[x+2,y])
            temp[y * 5 + x] = ctx->state[y * 5 + x] ^
                              ((~ctx->state[y * 5 + (x + 1) % 5]) & ctx->state[y * 5 + (x + 2) % 5]);
        }
    }
    for (int32_t i = 0; i < 25; i++) {ctx->state[i] = temp[i];}
}

static void keccak_iota_step(keccak_ctx_t* ctx, int32_t round) {
    ctx->state[0] ^= keccak_f1600_constants[round];
}

static void keccak_f1600(keccak_ctx_t* ctx) {
    for (int32_t round = 0; round < 24; round++) {
        keccak_theta_step(ctx);
        keccak_rho_step(ctx);
        keccak_pi_step(ctx);
        keccak_chi_step(ctx);
        keccak_iota_step(ctx, round);
    }
}


static int8_t keccak_update(keccak_ctx_t* ctx, const uint8_t* data, size_t len) {
    if (ctx == NULL || data == NULL) {
        memory_free(ctx);
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];

        // When the buffer is full (reaches the bitrate), absorb it
        if (ctx->buffer_len == ctx->rate) {
            // XOR the buffer into the state (first 'rate' bytes)
            // Note: On amd64, you can optimize this by XORing 64-bit words
            for (size_t j = 0; j < ctx->rate; j++) {
                ((uint8_t*)ctx->state)[j] ^= ctx->buffer[j];
            }

            keccak_f1600(ctx);

            ctx->buffer_len = 0;
        }
    }

    return 0;
}

static uint8_t* keccak_final(keccak_ctx_t* ctx, uint8_t* hash, size_t hash_len) {
    if (ctx == NULL || hash == NULL) {
        memory_free(ctx);
        return NULL;
    }
    // --- 1. Finish the Absorb Phase ---
    // XOR the remaining "leftover" bytes in the buffer into the state
    for (size_t i = 0; i < ctx->buffer_len; i++) {
        ((uint8_t*)ctx->state)[i] ^= ctx->buffer[i];
    }

    // --- 2. Apply Multi-rate Padding (10*1) ---
    uint8_t pad_byte = (ctx->variant <= KECCAK_VARIANT_SHA3_512) ? 0x06 : 0x1F;

    // XOR the domain separator + first '1' of padding
    ((uint8_t*)ctx->state)[ctx->buffer_len] ^= pad_byte;

    // XOR the final '1' of the 10*1 padding at the end of the rate
    // This is the "1" at the other end of the sponge rate
    ((uint8_t*)ctx->state)[ctx->rate - 1] ^= 0x80;

    // --- 3. Final Permutation ---
    keccak_f1600(ctx);

    // --- 4. Squeeze Phase ---
    // (This part of your previous logic was actually correct for the XOF/SHAKE mode)
    size_t offset = 0;
    while (offset < hash_len) {
        size_t to_copy = (hash_len - offset < ctx->rate) ? (hash_len - offset) : ctx->rate;
        memory_memcopy(ctx->state, hash + offset, to_copy);
        offset += to_copy;

        if (offset < hash_len) {
            keccak_f1600(ctx);
        }
    }

    memory_free(ctx);
    return hash;
}

sha3_224_ctx_t* sha3_224_init(void) {
    return (sha3_224_ctx_t*)keccak_init(KECCAK_VARIANT_SHA3_224, 1152 / 8, 448 / 8, 224 / 8);
}

sha3_256_ctx_t* sha3_256_init(void) {
    return (sha3_256_ctx_t*)keccak_init(KECCAK_VARIANT_SHA3_256, 1088 / 8, 512 / 8, 256 / 8);
}

sha3_384_ctx_t* sha3_384_init(void) {
    return (sha3_384_ctx_t*)keccak_init(KECCAK_VARIANT_SHA3_384, 832 / 8, 768 / 8, 384 / 8);
}

sha3_512_ctx_t* sha3_512_init(void) {
    return (sha3_512_ctx_t*)keccak_init(KECCAK_VARIANT_SHA3_512, 576 / 8, 1024 / 8, 512 / 8);
}

shake128_ctx_t* shake128_init(void) {
    return (shake128_ctx_t*)keccak_init(KECCAK_VARIANT_SHAKE128, 1344 / 8, 256 / 8, 0);
}

shake256_ctx_t* shake256_init(void) {
    return (shake256_ctx_t*)keccak_init(KECCAK_VARIANT_SHAKE256, 1088 / 8, 512 / 8, 0);
}

int8_t sha3_224_update(sha3_224_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

int8_t sha3_256_update(sha3_256_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

int8_t sha3_384_update(sha3_384_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

int8_t sha3_512_update(sha3_512_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

int8_t shake128_update(shake128_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

int8_t shake256_update(shake256_ctx_t* ctx, const uint8_t* data, size_t len) {
    return keccak_update((keccak_ctx_t*)ctx, data, len);
}

uint8_t* sha3_224_final(sha3_224_ctx_t* ctx) {
    uint8_t* hash = memory_malloc(((keccak_ctx_t*)ctx)->output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, ((keccak_ctx_t*)ctx)->output_len);
}

uint8_t* sha3_256_final(sha3_256_ctx_t* ctx) {
    uint8_t* hash = memory_malloc(((keccak_ctx_t*)ctx)->output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, ((keccak_ctx_t*)ctx)->output_len);
}

uint8_t* sha3_384_final(sha3_384_ctx_t* ctx) {
    uint8_t* hash = memory_malloc(((keccak_ctx_t*)ctx)->output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, ((keccak_ctx_t*)ctx)->output_len);
}

uint8_t* sha3_512_final(sha3_512_ctx_t* ctx) {
    uint8_t* hash = memory_malloc(((keccak_ctx_t*)ctx)->output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, ((keccak_ctx_t*)ctx)->output_len);
}

uint8_t* shake128_final(shake128_ctx_t* ctx, size_t output_len) {
    uint8_t* hash = memory_malloc(output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, output_len);
}

uint8_t* shake256_final(shake256_ctx_t* ctx, size_t output_len) {
    uint8_t* hash = memory_malloc(output_len);
    return keccak_final((keccak_ctx_t*)ctx, hash, output_len);
}

uint8_t* sha3_224_hash(const uint8_t* data, size_t length) {
    sha3_224_ctx_t* ctx = sha3_224_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(sha3_224_update(ctx, data, length) != 0) {
        return NULL;
    }
    uint8_t* hash = sha3_224_final(ctx);
    return hash;
}

uint8_t* sha3_256_hash(const uint8_t* data, size_t length) {
    sha3_256_ctx_t* ctx = sha3_256_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(sha3_256_update(ctx, data, length) != 0) {
        return NULL;
    }
    uint8_t* hash = sha3_256_final(ctx);
    return hash;
}

uint8_t* sha3_384_hash(const uint8_t* data, size_t length) {
    sha3_384_ctx_t* ctx = sha3_384_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(sha3_384_update(ctx, data, length) != 0) {
        return NULL;
    }
    uint8_t* hash = sha3_384_final(ctx);
    return hash;
}

uint8_t* sha3_512_hash(const uint8_t* data, size_t length) {
    sha3_512_ctx_t* ctx = sha3_512_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(sha3_512_update(ctx, data, length) != 0) {
        return NULL;
    }
    uint8_t* hash = sha3_512_final(ctx);
    return hash;
}

uint8_t* shake128_hash(const uint8_t* data, size_t data_len, size_t output_len) {
    shake128_ctx_t* ctx = shake128_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(shake128_update(ctx, data, data_len) != 0) {
        return NULL;
    }
    uint8_t* hash = shake128_final(ctx, output_len);
    return hash;
}

uint8_t* shake256_hash(const uint8_t* data, size_t data_len, size_t output_len) {
    shake256_ctx_t* ctx = shake256_init();
    if(ctx == NULL) {
        return NULL;
    }
    if(shake256_update(ctx, data, data_len) != 0) {
        return NULL;
    }
    uint8_t* hash = shake256_final(ctx, output_len);
    return hash;
}

sha3_224_ctx_t* sha3_224_clone(const sha3_224_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (sha3_224_ctx_t*)new_ctx;
}

sha3_256_ctx_t* sha3_256_clone(const sha3_256_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (sha3_256_ctx_t*)new_ctx;
}

sha3_384_ctx_t* sha3_384_clone(const sha3_384_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (sha3_384_ctx_t*)new_ctx;
}

sha3_512_ctx_t* sha3_512_clone(const sha3_512_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (sha3_512_ctx_t*)new_ctx;
}

shake128_ctx_t* shake128_clone(const shake128_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (shake128_ctx_t*)new_ctx;
}

shake256_ctx_t* shake256_clone(const shake256_ctx_t* ctx) {
    if(ctx == NULL) {
        return NULL;
    }
    keccak_ctx_t* new_ctx = memory_malloc(sizeof(keccak_ctx_t));
    if(new_ctx == NULL) {
        return NULL;
    }
    memory_memcopy(ctx, new_ctx, sizeof(keccak_ctx_t));
    return (shake256_ctx_t*)new_ctx;
}
