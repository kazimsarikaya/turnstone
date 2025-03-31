/**
 * @file random.64.c
 * @brief Random number generator using xoroshiro algorithms.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <random.h>
#include <xxhash.h>
#include <cpu.h>

MODULE("turnstone.lib.random");

static uint64_t random_xoroshiro_seed = 0;
static uint64_t random_xoroshiro_state[4] = {};
static boolean_t rdrand_supported = false;
static boolean_t rdrand_initialized = false;

static inline uint64_t random_xoroshiro_rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t random_xoroshiro_state_next(void) {
    uint64_t z = (random_xoroshiro_seed += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t random_xoroshiro_next(void) {
    const uint64_t result = random_xoroshiro_rotl(random_xoroshiro_state[0] + random_xoroshiro_state[3], 23) + random_xoroshiro_state[0];

    const uint64_t t = random_xoroshiro_state[1] << 17;

    random_xoroshiro_state[2] ^= random_xoroshiro_state[0];
    random_xoroshiro_state[3] ^= random_xoroshiro_state[1];
    random_xoroshiro_state[1] ^= random_xoroshiro_state[2];
    random_xoroshiro_state[0] ^= random_xoroshiro_state[3];

    random_xoroshiro_state[2] ^= t;

    random_xoroshiro_state[3] = random_xoroshiro_rotl(random_xoroshiro_state[3], 45);

    return result;
}

void srand(uint64_t seed) {
    random_xoroshiro_seed ^= seed;
    random_xoroshiro_seed = xxhash64_hash(&random_xoroshiro_seed, sizeof(random_xoroshiro_seed));
    random_xoroshiro_state[0] = random_xoroshiro_state_next();
    random_xoroshiro_state[1] = random_xoroshiro_state_next();
    random_xoroshiro_state[2] = random_xoroshiro_state_next();
    random_xoroshiro_state[3] = random_xoroshiro_state_next();
}

uint32_t rand(void) {
    if(!rdrand_initialized) {
        rdrand_supported = cpu_check_rdrand() == 0;
        rdrand_initialized = true;
    }

    if(!rdrand_supported) {
        return random_xoroshiro_next() >> 32;
    }

    boolean_t valid = false;
    uint32_t result = 0;

    while(!valid) {
        // if cf is set, then the value is valid
        asm volatile ("rdrand %0; setc %1" : "=r" (result), "=qm" (valid));
    }

    return result;
}

uint64_t rand64(void) {
    if(!rdrand_initialized) {
        rdrand_supported = cpu_check_rdrand() == 0;
        rdrand_initialized = true;
    }

    if(!rdrand_supported) {
        return random_xoroshiro_next();
    }

    boolean_t valid = false;
    uint64_t result = 0;

    while(!valid) {
        // if cf is set, then the value is valid
        asm volatile ("rdrand %0; setc %1" : "=r" (result), "=qm" (valid));

    }

    return result;
}
