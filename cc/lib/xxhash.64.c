/**
 * @file xxhash.64.c
 * @brief xxHash - Extremely Fast Hash algorithm implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <xxhash.h>
#include <utils.h>
#include <memory.h>

/*! module name */
MODULE("turnstone.lib.xxhash");

/*! xxHash 64-bit prime constant 1 */
#define XXHASH64_PRIME1  11400714785074694791ULL
/*! xxHash 64-bit prime constant 2 */
#define XXHASH64_PRIME2  14029467366897019727ULL
/*! xxHash 64-bit prime constant 3 */
#define XXHASH64_PRIME3   1609587929392839161ULL
/*! xxHash 64-bit prime constant 4 */
#define XXHASH64_PRIME4   9650029242287828579ULL
/*! xxHash 64-bit prime constant 5 */
#define XXHASH64_PRIME5   2870177450012600261ULL

/*! xxHash 64-bit max buffer size */
#define XXHASH64_MAXBUFFERSIZE  32

/**
 * @struct xxhash64_context_t
 * @brief xxHash 64-bit context structure
 */
typedef struct xxhash64_context_t {
    uint64_t state[4]; ///< state
    uint8_t  buffer[XXHASH64_MAXBUFFERSIZE]; ///< buffer
    int64_t  buffer_size; ///< buffer size
    uint64_t total_length; ///< total length
} xxhash64_context_t; ///< xxHash 64-bit context type


/**
 * @brief process single 64-bit block
 * @param[in] prev previous value
 * @param[in] curr current value
 * @return processed value
 */
static inline uint64_t xxhash64_process_single(uint64_t prev, uint64_t curr) {
    return ROTLEFT64(prev + curr * XXHASH64_PRIME2, 31) * XXHASH64_PRIME1;
}

/**
 * @brief process 64-bit block
 * @param[in] data data
 * @param[in,out] state0 state 0
 * @param[in,out] state1 state 1
 * @param[in,out] state2 state 2
 * @param[in,out] state3 state 3
 */
static inline void xxhash64_process(const void* data, uint64_t* state0, uint64_t* state1, uint64_t* state2, uint64_t* state3) {
    const uint64_t* block = (const uint64_t*)data;

    *state0 = xxhash64_process_single(*state0, block[0]);
    *state1 = xxhash64_process_single(*state1, block[1]);
    *state2 = xxhash64_process_single(*state2, block[2]);
    *state3 = xxhash64_process_single(*state3, block[3]);
}

static inline xxhash64_context_t* xxhhash64_init_without_malloc(xxhash64_context_t* ctx, uint64_t seed) {
    ctx->state[0] = seed + XXHASH64_PRIME1 + XXHASH64_PRIME2;
    ctx->state[1] = seed + XXHASH64_PRIME2;
    ctx->state[2] = seed;
    ctx->state[3] = seed - XXHASH64_PRIME1;
    ctx->buffer_size  = 0;
    ctx->total_length = 0;

    return ctx;
}

static uint64_t xxhash64_final_without_free(xxhash64_context_t* ctx) {

    if(ctx == NULL) {
        return 0;
    }

    uint64_t result;

    if (ctx->total_length >= XXHASH64_MAXBUFFERSIZE) {
        result = ROTLEFT64(ctx->state[0],  1) +
                 ROTLEFT64(ctx->state[1],  7) +
                 ROTLEFT64(ctx->state[2], 12) +
                 ROTLEFT64(ctx->state[3], 18);

        result = (result ^ xxhash64_process_single(0, ctx->state[0])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        result = (result ^ xxhash64_process_single(0, ctx->state[1])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        result = (result ^ xxhash64_process_single(0, ctx->state[2])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
        result = (result ^ xxhash64_process_single(0, ctx->state[3])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
    } else {
        result = ctx->state[2] + XXHASH64_PRIME5;
    }

    result += ctx->total_length;

    const unsigned char* data = ctx->buffer;
    const unsigned char* stop = data + ctx->buffer_size;

    for (; data + 8 <= stop; data += 8) {
        result = ROTLEFT64(result ^ xxhash64_process_single(0, *(uint64_t*)data), 27) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
    }

    if (data + 4 <= stop) {
        result = ROTLEFT64(result ^ (*(uint32_t*)data) * XXHASH64_PRIME1, 23) * XXHASH64_PRIME2 + XXHASH64_PRIME3;
        data  += 4;
    }

    while (data != stop) {
        result = ROTLEFT64(result ^ (*data) * XXHASH64_PRIME5, 11) * XXHASH64_PRIME1;
        data++;
    }

    result ^= result >> 33;
    result *= XXHASH64_PRIME2;
    result ^= result >> 29;
    result *= XXHASH64_PRIME3;
    result ^= result >> 32;

    return result;
}

uint64_t xxhash64_hash_with_seed(const void* input, uint64_t length, uint64_t seed) {
    xxhash64_context_t ctx = {0};

    xxhhash64_init_without_malloc(&ctx, seed);
    xxhash64_update(&ctx, input, length);

    return xxhash64_final_without_free(&ctx);
}

xxhash64_context_t* xxhash64_init(uint64_t seed) {
    xxhash64_context_t* ctx = memory_malloc(sizeof(xxhash64_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    return xxhhash64_init_without_malloc(ctx, seed);
}

uint64_t xxhash64_final(xxhash64_context_t* ctx) {

    if(ctx == NULL) {
        return 0;
    }

    uint64_t result = xxhash64_final_without_free(ctx);

    memory_free(ctx);

    return result;
}


int8_t xxhash64_update(xxhash64_context_t* ctx, const void* input, uint64_t length) {

    if(ctx == NULL) {
        return NULL;
    }

    if (!input || length == 0) {
        return -1;
    }

    ctx->total_length += length;

    const unsigned char* data = (const unsigned char*)input;

    if (ctx->buffer_size + length < XXHASH64_MAXBUFFERSIZE) {
        while (length-- > 0) {
            ctx->buffer[ctx->buffer_size++] = *data++;
        }

        return 0;
    }

    if (ctx->buffer_size > 0) {
        uint64_t to_fill = XXHASH64_MAXBUFFERSIZE - ctx->buffer_size;

        while (ctx->buffer_size < XXHASH64_MAXBUFFERSIZE) {
            ctx->buffer[ctx->buffer_size++] = *data++;
        }

        length -= to_fill;

        xxhash64_process(ctx->buffer, &ctx->state[0], &ctx->state[1], &ctx->state[2], &ctx->state[3]);
    }

    uint64_t s0 = ctx->state[0], s1 = ctx->state[1], s2 = ctx->state[2], s3 = ctx->state[3];

    int64_t nb_blocks = length / XXHASH64_MAXBUFFERSIZE;

    while (nb_blocks-- > 0)
    {
        xxhash64_process(data, &s0, &s1, &s2, &s3);
        data += XXHASH64_MAXBUFFERSIZE;
    }

    ctx->state[0] = s0; ctx->state[1] = s1; ctx->state[2] = s2; ctx->state[3] = s3;

    ctx->buffer_size = length % XXHASH64_MAXBUFFERSIZE;

    for (int64_t i = 0; i < ctx->buffer_size; i++) {
        ctx->buffer[i] = data[i];
    }


    return 0;
}

/*! xxHash 32-bit prime constant 1 */
#define XXHASH32_PRIME1  2654435761U
/*! xxHash 32-bit prime constant 2 */
#define XXHASH32_PRIME2  2246822519U
/*! xxHash 32-bit prime constant 3 */
#define XXHASH32_PRIME3  3266489917U
/*! xxHash 32-bit prime constant 4 */
#define XXHASH32_PRIME4   668265263U
/*! xxHash 32-bit prime constant 5 */
#define XXHASH32_PRIME5   374761393U

/*! xxHash 32-bit max buffer size */
#define XXHASH32_MAXBUFFERSIZE  16

/**
 * @struct xxhash32_context_t
 * @brief xxhash32 context
 */
typedef struct xxhash32_context_t {
    uint32_t state[4]; ///< state
    uint8_t  buffer[XXHASH32_MAXBUFFERSIZE]; ///< buffer
    int64_t  buffer_size; ///< buffer size
    uint64_t total_length; ///< total length
} xxhash32_context_t; ///< xxhash32 context type

/**
 * @brief xxhash32 process single
 * @param[in] prev previous value
 * @param[in] curr current value
 * @return processed value
 */
static inline uint32_t xxhash32_process_single(uint32_t prev, uint32_t curr) {
    return ROTLEFT32(prev + curr * XXHASH32_PRIME2, 13) * XXHASH32_PRIME1;
}

/**
 * @brief xxhash32 process block
 * @param[in] data data
 * @param[in,out] state0 state 0
 * @param[in,out] state1 state 1
 * @param[in,out] state2 state 2
 * @param[in,out] state3 state 3
 */
static inline void xxhash32_process(const void* data, uint32_t* state0, uint32_t* state1, uint32_t* state2, uint32_t* state3) {
    const uint32_t* block = (const uint32_t*)data;

    *state0 = xxhash32_process_single(*state0, block[0]);
    *state1 = xxhash32_process_single(*state1, block[1]);
    *state2 = xxhash32_process_single(*state2, block[2]);
    *state3 = xxhash32_process_single(*state3, block[3]);
}

uint32_t xxhash32_hash_with_seed(const void* input, uint64_t length, uint32_t seed) {
    xxhash32_context_t* ctx = xxhash32_init(seed);
    xxhash32_update(ctx, input, length);

    return xxhash32_final(ctx);
}

xxhash32_context_t* xxhash32_init(uint32_t seed) {
    xxhash32_context_t* ctx = memory_malloc(sizeof(xxhash32_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->state[0] = seed + XXHASH32_PRIME1 + XXHASH32_PRIME2;
    ctx->state[1] = seed + XXHASH32_PRIME2;
    ctx->state[2] = seed;
    ctx->state[3] = seed - XXHASH32_PRIME1;
    ctx->buffer_size  = 0;
    ctx->total_length = 0;

    return ctx;
}

uint32_t xxhash32_final(xxhash32_context_t* ctx) {

    if(ctx == NULL) {
        return NULL;
    }

    uint32_t result = (uint32_t)ctx->total_length;

    if (ctx->total_length >= XXHASH32_MAXBUFFERSIZE) {
        result += ROTLEFT32(ctx->state[0],  1) +
                  ROTLEFT32(ctx->state[1],  7) +
                  ROTLEFT32(ctx->state[2], 12) +
                  ROTLEFT32(ctx->state[3], 18);
    } else {
        result += ctx->state[2] + XXHASH32_PRIME5;
    }

    const unsigned char* data = ctx->buffer;
    const unsigned char* stop = data + ctx->buffer_size;

    for (; data + 4 <= stop; data += 4) {
        result = ROTLEFT32(result + *(uint32_t*)data * XXHASH32_PRIME3, 17) * XXHASH32_PRIME4;
    }

    while (data != stop) {
        result = ROTLEFT32(result + *data * XXHASH32_PRIME5, 11) * XXHASH32_PRIME1;
        data++;
    }

    result ^= result >> 15;
    result *= XXHASH32_PRIME2;
    result ^= result >> 13;
    result *= XXHASH32_PRIME3;
    result ^= result >> 16;

    memory_free(ctx);

    return result;
}


int8_t xxhash32_update(xxhash32_context_t* ctx, const void* input, uint64_t length) {

    if(ctx == NULL) {
        return NULL;
    }

    if (!input || length == 0) {
        return -1;
    }

    ctx->total_length += length;

    const unsigned char* data = (const unsigned char*)input;

    if (ctx->buffer_size + ctx->total_length < XXHASH32_MAXBUFFERSIZE) {
        while (length-- > 0) {
            ctx->buffer[ctx->buffer_size++] = *data++;
        }

        return 0;
    }

    if (ctx->buffer_size > 0) {
        uint64_t to_fill = XXHASH32_MAXBUFFERSIZE - ctx->buffer_size;

        while (ctx->buffer_size < XXHASH32_MAXBUFFERSIZE) {
            ctx->buffer[ctx->buffer_size++] = *data++;
        }

        length -= to_fill;

        xxhash32_process(ctx->buffer, &ctx->state[0], &ctx->state[1], &ctx->state[2], &ctx->state[3]);
    }

    uint32_t s0 = ctx->state[0], s1 = ctx->state[1], s2 = ctx->state[2], s3 = ctx->state[3];

    int64_t nb_blocks = length / XXHASH32_MAXBUFFERSIZE;

    while (nb_blocks-- > 0)
    {
        xxhash32_process(data, &s0, &s1, &s2, &s3);
        data += XXHASH32_MAXBUFFERSIZE;
    }

    ctx->state[0] = s0; ctx->state[1] = s1; ctx->state[2] = s2; ctx->state[3] = s3;

    ctx->buffer_size = length % XXHASH32_MAXBUFFERSIZE;

    for (int32_t i = 0; i < ctx->buffer_size; i++) {
        ctx->buffer[i] = data[i];
    }


    return 0;
}
