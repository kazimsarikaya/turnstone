#include <xxhash.h>
#include <utils.h>
#include <memory.h>

#define XXHASH64_PRIME1  11400714785074694791ULL
#define XXHASH64_PRIME2  14029467366897019727ULL
#define XXHASH64_PRIME3   1609587929392839161ULL
#define XXHASH64_PRIME4   9650029242287828579ULL
#define XXHASH64_PRIME5   2870177450012600261ULL

#define XXHASH64_MAXBUFFERSIZE  32

typedef struct {
	uint64_t state[4];
	uint8_t buffer[XXHASH64_MAXBUFFERSIZE];
	uint64_t buffer_size;
	uint64_t total_length;
} xxhash64_internal_context_t;



static inline uint64_t xxhash64_process_single(uint64_t prev, uint64_t curr) {
	return ROTLEFT64(prev + curr * XXHASH64_PRIME2, 31) * XXHASH64_PRIME1;
}

static inline void xxhash64_process(const void* data, uint64_t* state0, uint64_t* state1, uint64_t* state2, uint64_t* state3) {
	const uint64_t* block = (const uint64_t*)data;

	*state0 = xxhash64_process_single(*state0, block[0]);
	*state1 = xxhash64_process_single(*state1, block[1]);
	*state2 = xxhash64_process_single(*state2, block[2]);
	*state3 = xxhash64_process_single(*state3, block[3]);
}

uint64_t xxhash64_hash_with_seed(const void* input, uint64_t length, uint64_t seed) {
	xxhash_context_t ctx = xxhash64_init(seed);
	xxhash64_update(ctx, input, length);

	return xxhash64_final(ctx);
}

xxhash_context_t xxhash64_init(uint64_t seed) {
	xxhash64_internal_context_t* ictx = memory_malloc(sizeof(xxhash64_internal_context_t));

	ictx->state[0] = seed + XXHASH64_PRIME1 + XXHASH64_PRIME2;
	ictx->state[1] = seed + XXHASH64_PRIME2;
	ictx->state[2] = seed;
	ictx->state[3] = seed - XXHASH64_PRIME1;
	ictx->buffer_size  = 0;
	ictx->total_length = 0;

	return (xxhash_context_t)ictx;
}

uint64_t xxhash64_final(xxhash_context_t ctx) {
	xxhash64_internal_context_t* ictx = (xxhash64_internal_context_t*)ctx;

	uint64_t result;

	if (ictx->total_length >= XXHASH64_MAXBUFFERSIZE) {
		result = ROTLEFT64(ictx->state[0],  1) +
		         ROTLEFT64(ictx->state[1],  7) +
		         ROTLEFT64(ictx->state[2], 12) +
		         ROTLEFT64(ictx->state[3], 18);

		result = (result ^ xxhash64_process_single(0, ictx->state[0])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
		result = (result ^ xxhash64_process_single(0, ictx->state[1])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
		result = (result ^ xxhash64_process_single(0, ictx->state[2])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
		result = (result ^ xxhash64_process_single(0, ictx->state[3])) * XXHASH64_PRIME1 + XXHASH64_PRIME4;
	} else {
		result = ictx->state[2] + XXHASH64_PRIME5;
	}

	result += ictx->total_length;

	const unsigned char* data = ictx->buffer;
	const unsigned char* stop = data + ictx->buffer_size;

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

	memory_free(ictx);

	return result;
}


int8_t xxhash64_update(xxhash_context_t ctx, const void* input, uint64_t length) {
	xxhash64_internal_context_t* ictx = (xxhash64_internal_context_t*)ctx;

	if (!input || length == 0) {
		return -1;
	}

	ictx->total_length += length;

	const unsigned char* data = (const unsigned char*)input;

	if (ictx->buffer_size + ictx->total_length < XXHASH64_MAXBUFFERSIZE) {
		while (length-- > 0) {
			ictx->buffer[ictx->buffer_size++] = *data++;
		}

		return 0;
	}

	const unsigned char* stop      = data + length;
	const unsigned char* stop_block = stop - XXHASH64_MAXBUFFERSIZE;

	if (ictx->buffer_size > 0) {
		while (ictx->buffer_size < XXHASH64_MAXBUFFERSIZE) {
			ictx->buffer[ictx->buffer_size++] = *data++;
		}

		xxhash64_process(ictx->buffer, &ictx->state[0], &ictx->state[1], &ictx->state[2], &ictx->state[3]);
	}

	uint64_t s0 = ictx->state[0], s1 = ictx->state[1], s2 = ictx->state[2], s3 = ictx->state[3];

	while (data <= stop_block)
	{
		xxhash64_process(data, &s0, &s1, &s2, &s3);
		data += 32;
	}

	ictx->state[0] = s0; ictx->state[1] = s1; ictx->state[2] = s2; ictx->state[3] = s3;

	ictx->buffer_size = stop - data;
	for (uint64_t i = 0; i < ictx->buffer_size; i++) {
		ictx->buffer[i] = data[i];
	}


	return 0;
}
