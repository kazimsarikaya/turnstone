/**
 * @file data.64.c
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


data_t* data_json_serialize(data_t* data) {
    UNUSED(data);
    return NULL;
}

data_t* data_json_deserialize(data_t* data) {
    UNUSED(data);
    return NULL;
}

data_t* data_bson_serialize(data_t* data, data_serialize_with_t sw){
    if(sw == DATA_SERIALIZE_WITH_NONE) {
        return NULL;
    }

    data_serialize_with_t org_sw = sw;

    buffer_t buf = buffer_new();

    uint8_t bc = 0;

    if(data->name == NULL) {
        sw &= ~DATA_SERIALIZE_WITH_NAME;
    }

    if(sw & DATA_SERIALIZE_WITH_FLAGS) {
        buffer_append_bytes(buf, (uint8_t*)&sw, sizeof(uint8_t));
    }

    if((sw & DATA_SERIALIZE_WITH_NAME) && data->name) {
        data_t* tmp_name = data_bson_serialize(data->name, DATA_SERIALIZE_WITH_TYPE | DATA_SERIALIZE_WITH_LENGTH);

        if(tmp_name == NULL) {
            buffer_destroy(buf);

            return NULL;
        }

        buffer_append_bytes(buf, (uint8_t*)tmp_name->value, tmp_name->length);

        memory_free(tmp_name->value);
        memory_free(tmp_name);
    }

    if(sw & DATA_SERIALIZE_WITH_TYPE) {
        buffer_append_bytes(buf, (uint8_t*)&data->type, sizeof(uint8_t));
    }

    if((sw & DATA_SERIALIZE_WITH_LENGTH) && data->type >= DATA_TYPE_STRING) {
        uint64_t len = data->length;

        if(data->type == DATA_TYPE_STRING) {
            len = strlen((char_t*)data->value);
        }

        bc = byte_count(len);

        buffer_append_byte(buf, bc);
        buffer_append_bytes(buf, (uint8_t*)&len, bc);
    }

    switch (data->type) {
    case DATA_TYPE_NULL:
        break;
    case DATA_TYPE_BOOLEAN:
    case DATA_TYPE_CHAR:
    case DATA_TYPE_INT8:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint8_t));
        break;
    case DATA_TYPE_INT16:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint16_t));
        break;
    case DATA_TYPE_INT32:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint32_t));
        break;
    case DATA_TYPE_INT64:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint64_t));
        break;
    case DATA_TYPE_FLOAT32:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(float32_t));
        break;
    case DATA_TYPE_FLOAT64:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(float64_t));
        break;
    case DATA_TYPE_STRING:
        buffer_append_bytes(buf, (uint8_t*)data->value, strlen((char_t*)data->value));
        break;
    case DATA_TYPE_DATA:
        for(uint64_t i = 0; i < data->length; i++) {
            data_t* tmp_data = data_bson_serialize(&(((data_t*)(data->value))[i]), org_sw);

            if(tmp_data == NULL) {
                buffer_destroy(buf);
                return NULL;
            }

            buffer_append_bytes(buf, (uint8_t*)tmp_data->value, tmp_data->length);

            memory_free(tmp_data->value);
            memory_free(tmp_data);
        }
        break;
    case DATA_TYPE_INT8_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint8_t));
        break;
    case DATA_TYPE_INT16_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint16_t));
        break;
    case DATA_TYPE_INT32_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint32_t));
        break;
    case DATA_TYPE_INT64_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint64_t));
        break;
    case DATA_TYPE_FLOAT32_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(float32_t));
        break;
    case DATA_TYPE_FLOAT64_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(float64_t));
        break;
    default:
        buffer_destroy(buf);
        return NULL;
    }


    data_t* res = memory_malloc(sizeof(data_t));

    uint64_t obuflen = 0;
    uint8_t* obuf = buffer_get_all_bytes(buf, &obuflen);

    if(!obuflen || !obuf) {
        memory_free(res);
        buffer_destroy(buf);

        return NULL;
    }

    bc = byte_count(obuflen);

    res->type = DATA_TYPE_INT8_ARRAY;
    res->length = 1 + bc + obuflen;

    uint8_t* ores = memory_malloc(res->length);

    if(!ores) {
        memory_free(obuf);
        memory_free(res);
        buffer_destroy(buf);

        return NULL;
    }

    ores[0] = bc;
    memory_memcopy((uint8_t*)&obuflen, ores + 1, bc);
    memory_memcopy(obuf, ores + 1 + bc, obuflen);
    memory_free(obuf);

    res->value = ores;

    buffer_destroy(buf);

    return res;
}

data_t* data_bson_deserialize(data_t* data, data_serialize_with_t sw) {
    if(data == NULL) {
        return NULL;
    }

    if(data->type != DATA_TYPE_INT8_ARRAY) {
        return NULL;
    }

    uint8_t bc = 0;
    uint64_t len = 0;
    uint8_t* blen = (uint8_t*)&len;
    uint64_t total_len = data->length;

    if(total_len < 1) {
        return NULL;
    }

    uint8_t* tmp_data = (uint8_t*)data->value;

    if(tmp_data == NULL) {
        return NULL;
    }

    bc = *tmp_data;

    if(bc > 8 || total_len < 1ULL + bc) {
        return NULL;
    }

    tmp_data++;
    total_len--;

    memory_memcopy(tmp_data, blen, bc);

    tmp_data += bc;
    total_len -= bc;

    if(total_len < len) {
        return NULL;
    }

    data_t* res = memory_malloc(sizeof(data_t));

    if(res == NULL) {
        return NULL;
    }

    if(sw & DATA_SERIALIZE_WITH_FLAGS) {
        sw = (data_serialize_with_t)*tmp_data;
        tmp_data++;
        total_len--;
    }

    if(sw & DATA_SERIALIZE_WITH_NAME) {
        data_t td = {DATA_TYPE_INT8_ARRAY, total_len, NULL, tmp_data};
        res->name = data_bson_deserialize(&td, DATA_SERIALIZE_WITH_TYPE | DATA_SERIALIZE_WITH_LENGTH);

        if(res->name == NULL) {
            goto catch_error;
        }

        if(total_len < res->name->length) {
            memory_free(res);

            return NULL;
        }

        bc = *tmp_data;
        tmp_data++;
        total_len--;
        len = 0;
        memory_memcopy(tmp_data, blen, bc);

        tmp_data += len + bc;
        total_len -= len + bc;
    }

    if(sw & DATA_SERIALIZE_WITH_TYPE) {
        if(total_len < 1) {
            goto catch_error;
        }

        res->type = *tmp_data;
        tmp_data++;
        total_len--;
    } else {
        res->type = DATA_TYPE_INT8_ARRAY;
    }

    if((sw & DATA_SERIALIZE_WITH_LENGTH) && res->type >= DATA_TYPE_STRING) {
        if(total_len < 1) {
            goto catch_error;
        }

        bc = *tmp_data;

        if(bc > 8 || total_len < 1ULL + bc) {
            goto catch_error;
        }

        tmp_data++;
        total_len--;

        len = 0;
        memory_memcopy(tmp_data, blen, bc);

        tmp_data += bc;
        total_len -= bc;

        if(total_len < len) {
            goto catch_error;
        }

        res->length = len;
    }

    if(res->length == 0 && res->type < DATA_TYPE_STRING) {
        switch (res->type) {
        case DATA_TYPE_NULL:
            break;
        case DATA_TYPE_BOOLEAN:
        case DATA_TYPE_CHAR:
        case DATA_TYPE_INT8:
            res->length = sizeof(uint8_t);
            break;
        case DATA_TYPE_INT16:
            res->length = sizeof(uint16_t);
            break;
        case DATA_TYPE_INT32:
            res->length = sizeof(uint32_t);
            break;
        case DATA_TYPE_INT64:
            res->length = sizeof(uint64_t);
            break;
        case DATA_TYPE_FLOAT32:
            res->length = sizeof(float32_t);
            break;
        case DATA_TYPE_FLOAT64:
            res->length = sizeof(float64_t);
            break;
        default:
            break;
        }
    }

    if(res->length == 0) {
        return res;
    }

    if(res->type == DATA_TYPE_DATA) {
        data_t* data_items = memory_malloc(sizeof(data_t) * res->length);

        for(uint64_t i = 0; i < res->length; i++) {
            data_t td = {DATA_TYPE_INT8_ARRAY, total_len, NULL, tmp_data};
            data_t* data_item = data_bson_deserialize(&td, sw);

            memory_memcopy(data_item, data_items + i, sizeof(data_t));

            memory_free(data_item);

            bc = *tmp_data;
            tmp_data++;
            total_len--;
            len = 0;
            memory_memcopy(tmp_data, blen, bc);

            tmp_data += len + bc;
            total_len -= len + bc;
        }

        res->value = data_items;

        return res;
    }

    uint64_t ilen = res->length;
    uint64_t mlen = ilen;

    switch (res->type) {
    case DATA_TYPE_STRING:
        mlen++;
        break;
    case DATA_TYPE_INT8_ARRAY:
        break;
    case DATA_TYPE_INT16_ARRAY:
        ilen *= sizeof(uint16_t);
        mlen *= sizeof(uint16_t);
        break;
    case DATA_TYPE_INT32_ARRAY:
        ilen *= sizeof(uint32_t);
        mlen *= sizeof(uint32_t);
        break;
    case DATA_TYPE_INT64_ARRAY:
        ilen *= sizeof(uint64_t);
        mlen *= sizeof(uint64_t);
        break;
    case DATA_TYPE_FLOAT32_ARRAY:
        ilen *= sizeof(float32_t);
        mlen *= sizeof(float32_t);
        break;
    case DATA_TYPE_FLOAT64_ARRAY:
        ilen *= sizeof(float64_t);
        mlen *= sizeof(float64_t);
        break;
    default:
        break;
    }

    if(total_len < ilen) {
        goto catch_error;
    }

    uint8_t* idata = memory_malloc(mlen);

    if(idata == NULL) {
        memory_free(res);

        return NULL;
    }

    memory_memcopy(tmp_data, idata, ilen);



    if(res->type < DATA_TYPE_STRING) {
        uint64_t tmp_int = 0;
        switch (res->type) {
        case DATA_TYPE_NULL:
            break;
        case DATA_TYPE_BOOLEAN:
        case DATA_TYPE_CHAR:
        case DATA_TYPE_INT8:
            tmp_int = (*((uint8_t*)idata));
            res->value = (void*)tmp_int;
            break;
        case DATA_TYPE_INT16:
            tmp_int = (*((uint16_t*)idata));
            res->value = (void*)tmp_int;
            break;
        case DATA_TYPE_INT32:
            tmp_int = (*((uint32_t*)idata));
            res->value = (void*)tmp_int;
            break;
        case DATA_TYPE_INT64:
            tmp_int = (*((uint64_t*)idata));
            res->value = (void*)tmp_int;
            break;
        case DATA_TYPE_FLOAT32:
            tmp_int = (*((uint32_t*)idata));
            res->value = (void*)tmp_int;
            break;
        case DATA_TYPE_FLOAT64:
            tmp_int = (*((uint64_t*)idata));
            res->value = (void*)tmp_int;
            break;
        default:
            break;
        }

        memory_free(idata);
    } else {
        res->value = idata;
    }

    return res;

catch_error:
    if(res->name) {
        if(res->name->type >= DATA_TYPE_STRING) {
            memory_free(res->name->value);
            memory_free(res->name);
        }
    }

    if(res->type >= DATA_TYPE_STRING) { // FIXME: DATA_TYPE_DATA memory leak
        memory_free(res->value);
    }

    memory_free(res);

    return NULL;
}

void data_free(data_t* data) {
    if(data->name) {
        data_free(data->name);
    }

    if(data->type >= DATA_TYPE_STRING) {
        if(data->type == DATA_TYPE_DATA) {
            data_t* ds = data->value;
            for(uint64_t i = 0; i < data->length; i++) {
                if(ds[i].name) {
                    data_free(ds[i].name);
                }

                if(ds[i].type >= DATA_TYPE_STRING) {
                    if(ds[i].type == DATA_TYPE_DATA) {
                        data_free(ds[i].value);
                    } else {
                        memory_free(ds[i].value);
                    }
                }
            }

            memory_free(ds);
        } else {
            memory_free(data->value);
        }
    }

    memory_free(data);
}
