/**
 * @file data.h
 * @brief data interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___DATA_H
#define ___DATA_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum data_type_t {
    DATA_TYPE_NULL=0,
    DATA_TYPE_BOOLEAN,
    DATA_TYPE_CHAR,
    DATA_TYPE_INT8,
    DATA_TYPE_INT16,
    DATA_TYPE_INT32,
    DATA_TYPE_INT64,
    DATA_TYPE_FLOAT32,
    DATA_TYPE_FLOAT64,
    DATA_TYPE_STRING,
    DATA_TYPE_DATA,
    DATA_TYPE_INT8_ARRAY,
    DATA_TYPE_INT16_ARRAY,
    DATA_TYPE_INT32_ARRAY,
    DATA_TYPE_INT64_ARRAY,
    DATA_TYPE_FLOAT32_ARRAY,
    DATA_TYPE_FLOAT64_ARRAY,
    DATA_TYPE_NR,
} data_type_t;

typedef struct data_t {
    data_type_t    type;
    uint64_t       length;
    struct data_t* name;
    void*          value;
}data_t;

data_t* data_bson_serialize(data_t* data);
data_t* data_bson_deserialize(data_t* data);
data_t* data_json_serialize(data_t* data);
data_t* data_json_deserialize(data_t* data);

void data_free(data_t* data);

#ifdef __cplusplus
}
#endif

#endif
