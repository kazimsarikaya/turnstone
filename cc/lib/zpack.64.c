/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <zpack.h>
#include <utils.h>

MODULE("turnstone.lib");

#define ZPACK_MAX_MATCH (0x3f + 0x40 + 4)
#define ZPACK_MIN_MATCH (4)
#define ZPACK_WINDOW_SIZE 16383
#define ZPACK_HASHTABLE_SIZE 15
#define ZPACK_HASHTABLE_MUL 2654435761U
#define ZPACK_HASHTABLE_PREV_SIZE 16384
#define ZPACK_NO_POS (-1)


typedef struct zpack_match_t {
    int32_t best_size;
    int64_t best_pos;
} zpack_match_t;

typedef struct zpack_hashtable_t {
    int64_t prev[ZPACK_HASHTABLE_PREV_SIZE];
    int64_t head[1ULL << ZPACK_HASHTABLE_SIZE];
} zpack_hashtable_t;


static inline uint32_t zpack_hash4(uint32_t data) {
    return (data * ZPACK_HASHTABLE_MUL) >> (32 - ZPACK_HASHTABLE_SIZE);
}

static inline void zpack_hash_insert(int64_t pos, uint32_t h, zpack_hashtable_t* ht) {
    ht->prev[pos % ZPACK_WINDOW_SIZE] = ht->head[h];
    ht->head[h] = pos;
}

static zpack_match_t zpack_find_bestmatch (buffer_t in, int64_t in_len, int64_t in_p, zpack_hashtable_t* ht) {
    int64_t max_match = MIN(in_len - in_p, ZPACK_MAX_MATCH);
    int64_t start = in_p - ZPACK_WINDOW_SIZE;

    if(start < 0) {
        start = 0;
    }

    uint32_t hash = buffer_peek_ints(in, ZPACK_MIN_MATCH);
    hash = zpack_hash4(hash);

    int64_t best_size = 0;
    int64_t best_pos = -1;
    int64_t i = ht->head[hash];
    int64_t max_match_step = 4096;

    while(i != ZPACK_NO_POS && max_match_step > 0) {
        if(i < start) {
            break;
        }

        if(best_size >= 32) {
            max_match_step >>= 2;
        }

        max_match_step--;

        int64_t j = 0;
        boolean_t not_matched = false;

        for(; j < best_size && j < max_match; j++) {
            if(buffer_peek_byte_at_position(in, i + j) != buffer_peek_byte_at_position(in, in_p + j)) {
                not_matched = true;

                break;
            }
        }

        if(not_matched) {
            i = ht->prev[i % ZPACK_HASHTABLE_PREV_SIZE];

            continue;
        }

        for(; j < max_match; j++) {
            if(buffer_peek_byte_at_position(in, i + j) != buffer_peek_byte_at_position(in, in_p + j)) {
                break;
            }
        }

        if(j >= ZPACK_MIN_MATCH && j > best_size) {
            best_size = j;
            best_pos = i;
        }

        if(j == max_match) {
            break;
        }

        int64_t prev_i = ht->prev[i % ZPACK_HASHTABLE_PREV_SIZE];

        if(prev_i == i) {
            break;
        }

        i = prev_i;
    }

    zpack_hash_insert(in_p, hash, ht);

    return (zpack_match_t){.best_size = best_size, .best_pos = best_pos};
}

int64_t zpack_pack (buffer_t in, buffer_t out) {
    zpack_hashtable_t* ht = memory_malloc(sizeof(zpack_hashtable_t));

    if(!ht) {
        return 0;
    }

    memory_memset(ht->head, 0xFF, sizeof(ht->head));

    buffer_t individuals = buffer_new_with_capacity(NULL, 257);
    int64_t in_len = buffer_get_length(in);

    while (buffer_remaining(in)) {
        int64_t in_p = (int64_t)buffer_get_position(in);

        zpack_match_t zpm = zpack_find_bestmatch(in, in_len, in_p, ht);

        if(zpm.best_pos != -1 && zpm.best_size > 3) {
            /* copy */
            if (buffer_get_length(individuals)) {
                out = buffer_append_byte(out, (buffer_get_length(individuals) - 1) | 0xC0);

                out = buffer_append_buffer(out, individuals);

                buffer_reset(individuals);
            }

            int32_t offset = in_p - zpm.best_pos;

            if(!buffer_seek(in, zpm.best_size, BUFFER_SEEK_DIRECTION_CURRENT)) {
                buffer_destroy(individuals);
                memory_free(ht);

                return -1;
            }

            zpm.best_size -= 4;

            out = buffer_append_byte(out, zpm.best_size);

            if(offset > 0xBF) {
                out = buffer_append_byte(out, (offset >> 8) | 0xC0);
                out = buffer_append_byte(out, offset & 0xFF);
            } else {
                out = buffer_append_byte(out, offset);
            }

        } else {
            /* individual bytes */
            individuals = buffer_append_byte(individuals, buffer_get_byte(in));

            if (buffer_get_length(individuals) == 0x40) {
                out = buffer_append_byte(out, (buffer_get_length(individuals) - 1) | 0xC0);

                out = buffer_append_buffer(out, individuals);

                buffer_reset(individuals);
            }
        }
    }

    if (buffer_get_length(individuals)) {
        out = buffer_append_byte(out, (buffer_get_length(individuals) - 1) | 0xC0);

        out = buffer_append_buffer(out, individuals);

        buffer_reset(individuals);
    }

    buffer_destroy(individuals);
    memory_free(ht);

    return buffer_get_length(out);
}

int64_t zpack_unpack(buffer_t in, buffer_t out) {
    while (buffer_remaining(in)) {

        int32_t size = buffer_get_byte(in);

        if ((size & 0xC0) == 0xC0) {
            size &= 0x3f;
            size++;

            uint8_t* data = buffer_get_bytes(in, size);
            out = buffer_append_bytes(out, data, size);

            memory_free(data);

        } else {
            int32_t offset = buffer_get_byte(in);

            if((offset & 0xC0) == 0xC0) {
                offset &= 0x3f;
                offset = (offset << 8) | buffer_get_byte(in);
            }

            size += 3;

            int64_t o_p = buffer_get_position(out) - offset;

            while(size-- >= 0 ) {
                out = buffer_append_byte(out, buffer_peek_byte_at_position(out, o_p));
                o_p++;
            }
        }
    }

    return buffer_get_length(out);
}
