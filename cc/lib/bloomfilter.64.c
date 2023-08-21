/**
 * @file bloomfilter.64.c
 * @brief bloom filter implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#include <bloomfilter.h>
#include <math.h>
#include <xxhash.h>
#include <memory.h>
#include <random.h>

MODULE("turnstone.lib");

/**
 * @struct bloomfilter_t
 * @brief bloom filter struct
 */
typedef struct bloomfilter_t {
    uint64_t  entry_count; ///< entry count at filter
    float64_t bpe; ///< bit per entry
    float64_t error; ///< error rate for false positive
    uint64_t  hash_count;  ///< how much hashing
    uint64_t  hash_seed; ///< xxhash seed
    uint64_t  bit_count; ///< bit count at array
    uint64_t* bits; ///bit array
}bloomfilter_t;

/**
 * @brief checks or adds given value to the given bloom filter
 * @param[in] bf bloom filter
 * @param[in] data data to add
 * @param[in] add if true then add or check only
 * @return check result
 *
 */
boolean_t bloomfilter_check_or_add(bloomfilter_t* bf, data_t* data, boolean_t add);

bloomfilter_t* bloomfilter_new(uint64_t entry_count, float64_t error) {
    if(!entry_count || (error < 0 || error >= 1)) {
        return NULL;
    }

    bloomfilter_t* res = memory_malloc(sizeof(bloomfilter_t));

    if(!res) {
        return res;
    }

    res->entry_count = entry_count;
    res->error = error;
    res->bpe = -math_log(error) / math_power(LN2, 2);
    res->hash_count = math_ceil(LN2 * res->bpe);

    res->hash_seed = rand();


    float64_t dec = entry_count;
    res->bit_count = (uint64_t)(dec * res->bpe);

    uint64_t bc = (res->bit_count + 63) / 64;

    res->bits = memory_malloc(bc * sizeof(uint64_t));

    if(!res->bits) {
        memory_free(res);

        return NULL;
    }

    return res;
}

boolean_t bloomfilter_destroy(bloomfilter_t* bf) {
    if(!bf) {
        return true;
    }

    memory_free(bf->bits);
    memory_free(bf);
    return true;
}

boolean_t bloomfilter_check_or_add(bloomfilter_t* bf, data_t* data, boolean_t add) {
    if(!bf || !data) {
        return false;
    }

    if(data->type != DATA_TYPE_INT8_ARRAY || !data->length) {
        return false;
    }

    uint64_t hits = 0;

    uint64_t a = xxhash64_hash_with_seed((uint8_t*)data->value, data->length, bf->hash_seed);
    uint64_t b = xxhash64_hash_with_seed((uint8_t*)data->value, data->length, a);

    uint64_t x = 0;

    for(uint64_t i = 0; i < bf->hash_count; i++) {
        x = (a + b * i) % bf->bit_count;
        boolean_t check = bit_test(bf->bits + (x / 64), x % 64);
        if(add) {
            bit_set(bf->bits + (x / 64), x % 64);
            hits++;
        } else if(check) {
            hits++;
        } else {
            return false;
        }
    }

    if(hits == bf->hash_count) {
        return true;
    }

    return false;
}


boolean_t bloomfilter_check(bloomfilter_t* bf, data_t* data) {
    return bloomfilter_check_or_add(bf, data, false);
}

boolean_t bloomfilter_add(bloomfilter_t* bf, data_t* data) {
    return bloomfilter_check_or_add(bf, data, true);
}

data_t* bloomfilter_serialize(bloomfilter_t* bf) {
    if(!bf) {
        return NULL;
    }

    data_t d = {0};
    d.type = DATA_TYPE_DATA;
    d.length = 7;

    data_t* fields = memory_malloc(sizeof(data_t) * d.length);

    if(!fields) {
        return NULL;
    }

    fields[0].type = DATA_TYPE_INT64;
    fields[0].value = (void*)bf->entry_count;

    uint64_t tmp = 0;
    memory_memcopy(&bf->bpe, &tmp, sizeof(uint64_t));
    fields[1].type = DATA_TYPE_FLOAT64;
    fields[1].value = (void*)tmp;

    tmp = 0;
    memory_memcopy(&bf->error, &tmp, sizeof(uint64_t));
    fields[2].type = DATA_TYPE_FLOAT64;
    fields[2].value = (void*)tmp;

    fields[3].type = DATA_TYPE_INT64;
    fields[3].value = (void*)bf->hash_count;


    fields[4].type = DATA_TYPE_INT64;
    fields[4].value = (void*)bf->hash_seed;


    fields[5].type = DATA_TYPE_INT64;
    fields[5].value = (void*)bf->bit_count;

    fields[6].type = DATA_TYPE_INT64_ARRAY;
    fields[6].value = bf->bits;
    fields[6].length = (bf->bit_count + 63) / 64;

    d.value = fields;

    data_t* res = data_bson_serialize(&d, DATA_SERIALIZE_WITH_LENGTH | DATA_SERIALIZE_WITH_TYPE);

    memory_free(fields);

    return res;
}

bloomfilter_t* bloomfilter_deserialize(data_t* data) {
    if(!data) {
        return NULL;
    }

    if(data->type != DATA_TYPE_INT8_ARRAY || !data->length) {
        return NULL;
    }

    data_t* bf_data = data_bson_deserialize(data, DATA_SERIALIZE_WITH_LENGTH | DATA_SERIALIZE_WITH_TYPE);

    if(!bf_data) {
        return NULL;
    }

    if(bf_data->length != 7 || bf_data->value == NULL) {
        data_free(bf_data);

        return NULL;
    }

    data_t* fields = (data_t*)bf_data->value;

    uint64_t entry_count = (uint64_t)fields[0].value;
    float64_t bpe = 0;
    uint64_t tmp = (uint64_t)fields[1].value;
    memory_memcopy(&tmp, &bpe, sizeof(uint64_t));
    float64_t error = 0;
    tmp = (uint64_t)fields[2].value;
    memory_memcopy(&tmp, &error, sizeof(uint64_t));
    uint64_t hash_count = (uint64_t)fields[3].value;
    uint64_t hash_seed = (uint64_t)fields[4].value;
    uint64_t bit_count = (uint64_t)fields[5].value;
    uint64_t* bits = (uint64_t*)fields[6].value;

    bloomfilter_t* res = memory_malloc(sizeof(bloomfilter_t));

    if(!res) {
        data_free(bf_data);

        return NULL;
    }

    res->entry_count = entry_count;
    res->bpe = bpe;
    res->error = error;
    res->hash_count = hash_count;
    res->hash_seed = hash_seed;
    res->bit_count = bit_count;
    res->bits = bits;

    memory_free(fields);
    memory_free(bf_data);

    return res;
}
