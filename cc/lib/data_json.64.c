/**
 * @file data_json.64.c
 * @brief data store implementation interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <data.h>
#include <memory.h>
#include <buffer.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.lib");


int8_t data_json_serialize_with_buffer(buffer_t* buf, data_t* data);

data_t* data_json_serialize(data_t* data) {
    buffer_t* buf = buffer_new();

    if(buf == NULL) {
        return NULL;
    }

    if(data_json_serialize_with_buffer(buf, data) == -1) {
        buffer_destroy(buf);
        return NULL;
    }

    uint64_t obuflen = 0;
    uint8_t* obuf = buffer_get_all_bytes_and_destroy(buf, &obuflen);

    if(!obuflen || !obuf) {
        return NULL;
    }

    data_t* res = memory_malloc(sizeof(data_t));

    if(!res) {
        memory_free(obuf);

        return NULL;
    }

    res->type = DATA_TYPE_INT8_ARRAY;
    res->length = obuflen;
    res->value = obuf;

    return res;
}

static int8_t data_json_escape_string(buffer_t * buf, char_t* str) {
    if(str == NULL) {
        return 0;
    }

    // we need to escape the string
    // encode \r \n \t \b \f \v \0 etc. also need to escape " and \ characters
    // encode non-printable characters as \u00xx

    for(uint64_t i = 0; i < strlen(str); i++) {
        switch (str[i]) {
        case '\r':
            buffer_append_bytes(buf, (uint8_t*)"\\r", 2);
            break;
        case '\n':
            buffer_append_bytes(buf, (uint8_t*)"\\n", 2);
            break;
        case '\t':
            buffer_append_bytes(buf, (uint8_t*)"\\t", 2);
            break;
        case '\b':
            buffer_append_bytes(buf, (uint8_t*)"\\b", 2);
            break;
        case '\f':
            buffer_append_bytes(buf, (uint8_t*)"\\f", 2);
            break;
        case '\v':
            buffer_append_bytes(buf, (uint8_t*)"\\v", 2);
            break;
        case '\0':
            buffer_append_bytes(buf, (uint8_t*)"\\0", 2);
            break;
        case '"':
            buffer_append_bytes(buf, (uint8_t*)"\\\"", 2);
            break;
        case '\\':
            buffer_append_bytes(buf, (uint8_t*)"\\\\", 2);
            break;
        default:
            if(str[i] < 32 || str[i] > 126) {
                buffer_printf(buf, "\\u%04x", str[i]);
            } else {
                buffer_append_bytes(buf, (uint8_t*)&str[i], 1);
            }
            break;
        }
    }


    return 0;
}

int8_t data_json_serialize_with_buffer(buffer_t* buf, data_t* data) {
    if(data == NULL) {
        return 0;
    }

    // malformed name for json. json needs a name
    if(data->name == NULL ||
       data->name->type != DATA_TYPE_STRING ||
       strlen(data->name->value) == 0) {
        return -1;
    }

    buffer_append_bytes(buf, (uint8_t*)"{", 1);

    buffer_append_bytes(buf, (uint8_t*)"\"", 1);

    if(data_json_escape_string(buf, (char_t*)data->name->value) == -1) {
        return -1;
    }

    buffer_append_bytes(buf, (uint8_t*)"\"", 1);

    buffer_append_bytes(buf, (uint8_t*)":", 1);

    switch (data->type) {
    case DATA_TYPE_NULL:
        buffer_append_bytes(buf, (uint8_t*)"null", 4);
        break;
    case DATA_TYPE_BOOLEAN:
        if(data->value) {
            buffer_append_bytes(buf, (uint8_t*)"true", 4);
        } else {
            buffer_append_bytes(buf, (uint8_t*)"false", 5);
        }
        break;
    case DATA_TYPE_CHAR:
    case DATA_TYPE_INT8:
    case DATA_TYPE_INT16:
    case DATA_TYPE_INT32:
    case DATA_TYPE_INT64:
        buffer_printf(buf, "%llu", (uint64_t)data->value);
        break;
    case DATA_TYPE_FLOAT32:
    case DATA_TYPE_FLOAT64: {
        uint64_t tmp_val = (uint64_t)data->value;
        float64_t* tmp_val_ptr = (float64_t*)&tmp_val;
        buffer_printf(buf, "%.17lf", *tmp_val_ptr);
    }
    break;
    case DATA_TYPE_STRING:
        buffer_append_bytes(buf, (uint8_t*)"\"", 1);

        if(data_json_escape_string(buf, (char_t*)data->value) == -1) {
            return -1;
        }

        buffer_append_bytes(buf, (uint8_t*)"\"", 1);
        break;
    case DATA_TYPE_DATA: {
        uint64_t len = data->length;

        if(len == 0) {
            buffer_append_bytes(buf, (uint8_t*)"{}", 2);
        } else if(len == 1) {
            data_t* datas = (data_t*)data->value;

            if(data_json_serialize_with_buffer(buf, datas) == -1) {
                return -1;
            }
        } else {
            buffer_append_bytes(buf, (uint8_t*)"[", 1);

            data_t* datas = (data_t*)data->value;

            for(uint64_t i = 0; i < len; i++) {
                if(data_json_serialize_with_buffer(buf, datas + i) == -1) {
                    return -1;
                }

                if(i < len - 1) {
                    buffer_append_bytes(buf, (uint8_t*)",", 1);
                }
            }

            buffer_append_bytes(buf, (uint8_t*)"]", 1);
        }
    }
    break;
    case DATA_TYPE_INT8_ARRAY:
    case DATA_TYPE_INT16_ARRAY:
    case DATA_TYPE_INT32_ARRAY:
    case DATA_TYPE_INT64_ARRAY:
    case DATA_TYPE_FLOAT32_ARRAY:
    case DATA_TYPE_FLOAT64_ARRAY: {
        uint64_t len = data->length;
        buffer_append_bytes(buf, (uint8_t*)"[", 1);

        for(uint64_t i = 0; i < len; i++) {
            switch (data->type) {
            case DATA_TYPE_INT8_ARRAY:
                buffer_printf(buf, "%u", ((uint8_t*)data->value)[i]);
                break;
            case DATA_TYPE_INT16_ARRAY:
                buffer_printf(buf, "%u", ((uint16_t*)data->value)[i]);
                break;
            case DATA_TYPE_INT32_ARRAY:
                buffer_printf(buf, "%u", ((uint32_t*)data->value)[i]);
                break;
            case DATA_TYPE_INT64_ARRAY:
                buffer_printf(buf, "%llu", ((uint64_t*)data->value)[i]);
                break;
            case DATA_TYPE_FLOAT32_ARRAY:
                buffer_printf(buf, "%f", ((float32_t*)data->value)[i]);
                break;
            case DATA_TYPE_FLOAT64_ARRAY:
                buffer_printf(buf, "%lf", ((float64_t*)data->value)[i]);
                break;
            default:
                break;
            }

            if(i < len - 1) {
                buffer_append_bytes(buf, (uint8_t*)",", 1);
            }
        }

        buffer_append_bytes(buf, (uint8_t*)"]", 1);
    }
    break;
    default:
        return -1;
    }

    buffer_append_bytes(buf, (uint8_t*)"}", 1);

    return 0;
}

data_t* data_json_deserialize(data_t* data) {
    UNUSED(data);
    return NULL;
}
