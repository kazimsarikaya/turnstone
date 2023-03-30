/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <random.h>

uint64_t random_xoroshiro_seed = 0;
uint64_t random_xoroshiro_state[4] = {};

uint64_t random_xoroshiro_state_next(void);
uint64_t random_xoroshiro_next(void);

static inline uint64_t random_xoroshiro_rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t random_xoroshiro_state_next(void) {
    uint64_t z = (random_xoroshiro_seed += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint64_t random_xoroshiro_next(void) {
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
    random_xoroshiro_seed = seed;
    random_xoroshiro_state[0] = random_xoroshiro_state_next();
    random_xoroshiro_state[1] = random_xoroshiro_state_next();
    random_xoroshiro_state[2] = random_xoroshiro_state_next();
    random_xoroshiro_state[3] = random_xoroshiro_state_next();
}

uint32_t rand() {
    return random_xoroshiro_next() >> 32;
}
