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


typedef enum {
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

typedef struct data_s {
	data_type_t type;
	uint64_t length;
	struct data_s* name;
	void* value;
}data_t;

typedef enum {
	DATA_SERIALIZE_WITH_NONE=0,
	DATA_SERIALIZE_WITH_TYPE=1,
	DATA_SERIALIZE_WITH_LENGTH=2,
	DATA_SERIALIZE_WITH_NAME=4,
	DATA_SERIALIZE_WITH_FLAGS=8,
	DATA_SERIALIZE_WITH_ALL=DATA_SERIALIZE_WITH_TYPE | DATA_SERIALIZE_WITH_LENGTH | DATA_SERIALIZE_WITH_NAME | DATA_SERIALIZE_WITH_FLAGS,
} data_serialize_with_t;

data_t* data_bson_serialize(data_t* data, data_serialize_with_t sw);
data_t* data_bson_deserialize(data_t* data, data_serialize_with_t sw);
data_t* data_json_serialize(data_t* data);
data_t* data_json_deserialize(data_t* data);

#endif
