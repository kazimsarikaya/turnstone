/**
 * @file data_bson.64.c
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

int8_t  data_bson_serialize_with_buffer(buffer_t* buf, data_t* data, uint64_t* sub_len);
data_t* data_bson_deserialize_with_processed(data_t* data, uint64_t* processed);

data_t* data_bson_serialize(data_t* data) {
    buffer_t* buf = buffer_new();

    if(buf == NULL) {
        return NULL;
    }

    uint64_t len = 0;

    if(data_bson_serialize_with_buffer(buf, data, &len) == -1) {
        buffer_destroy(buf);
        return NULL;
    }

    uint64_t obuflen = 0;
    uint8_t* obuf = buffer_get_all_bytes_and_destroy(buf, &obuflen);

    if(!obuflen || !obuf) {
        return NULL;
    }

    if(len != obuflen) {
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

int8_t data_bson_serialize_with_buffer(buffer_t* buf, data_t* data, uint64_t* sub_len) {
    uint64_t len = 0;

    uint64_t len_pos = buffer_get_position(buf);

    buffer_append_bytes(buf, (uint8_t*)&len, sizeof(uint64_t));
    len += sizeof(uint64_t);

    if(data == NULL) {
        *sub_len = len; // len
        return 0;
    }


    buffer_append_bytes(buf, (uint8_t*)&data->type, sizeof(uint8_t));
    len += sizeof(uint8_t);

    uint64_t name_len = 0;

    if(data_bson_serialize_with_buffer(buf, data->name, &name_len) == -1) {
        return -1;
    }

    len += name_len;

    if(data->type >= DATA_TYPE_STRING) {
        uint64_t type_len = data->length;

        if(data->type == DATA_TYPE_STRING) {
            type_len = strlen((char_t*)data->value);
        }

        buffer_append_bytes(buf, (uint8_t*)&type_len, sizeof(uint64_t));
        len += sizeof(uint64_t);
    }

    switch (data->type) {
    case DATA_TYPE_NULL:
        break;
    case DATA_TYPE_BOOLEAN:
    case DATA_TYPE_CHAR:
    case DATA_TYPE_INT8:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint8_t));
        len += sizeof(uint8_t);
        break;
    case DATA_TYPE_INT16:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint16_t));
        len += sizeof(uint16_t);
        break;
    case DATA_TYPE_INT32:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint32_t));
        len += sizeof(uint32_t);
        break;
    case DATA_TYPE_INT64:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(uint64_t));
        len += sizeof(uint64_t);
        break;
    case DATA_TYPE_FLOAT32:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(float32_t));
        len += sizeof(float32_t);
        break;
    case DATA_TYPE_FLOAT64:
        buffer_append_bytes(buf, (uint8_t*)&data->value, sizeof(float64_t));
        len += sizeof(float64_t);
        break;
    case DATA_TYPE_STRING:
        buffer_append_bytes(buf, (uint8_t*)data->value, strlen((char_t*)data->value));
        len += strlen((char_t*)data->value);
        break;
    case DATA_TYPE_DATA: {
        data_t* datas = (data_t*)data->value;

        for(uint64_t i = 0; i < data->length; i++) {
            uint64_t item_len = 0;

            if(data_bson_serialize_with_buffer(buf, &datas[i], &item_len) == -1) {
                return -1;
            }

            len += item_len;
        }
    }
    break;
    case DATA_TYPE_INT8_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint8_t));
        len += data->length * sizeof(uint8_t);
        break;
    case DATA_TYPE_INT16_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint16_t));
        len += data->length * sizeof(uint16_t);
        break;
    case DATA_TYPE_INT32_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint32_t));
        len += data->length * sizeof(uint32_t);
        break;
    case DATA_TYPE_INT64_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(uint64_t));
        len += data->length * sizeof(uint64_t);
        break;
    case DATA_TYPE_FLOAT32_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(float32_t));
        len += data->length * sizeof(float32_t);
        break;
    case DATA_TYPE_FLOAT64_ARRAY:
        buffer_append_bytes(buf, (uint8_t*)data->value, data->length * sizeof(float64_t));
        len += data->length * sizeof(float64_t);
        break;
    default:
        return -1;
    }

    uint64_t pos = buffer_get_position(buf);

    buffer_seek(buf, len_pos, BUFFER_SEEK_DIRECTION_START);
    buffer_append_bytes(buf, (uint8_t*)&len, sizeof(uint64_t));
    buffer_seek(buf, pos, BUFFER_SEEK_DIRECTION_START);

    *sub_len = len;

    return 0;
}

data_t* data_bson_deserialize(data_t* data) {
    uint64_t processed = 0;

    data_t* res = data_bson_deserialize_with_processed(data, &processed);

    return res;
}

data_t* data_bson_deserialize_with_processed(data_t* data, uint64_t* processed) {
    if(data == NULL) {
        return NULL;
    }

    if(data->type != DATA_TYPE_INT8_ARRAY) {
        return NULL;
    }

    uint64_t l_processed = 0;
    uint64_t len = 0;
    uint64_t total_len = data->length;

    if(total_len < 8) {
        return NULL;
    }

    uint8_t* tmp_data = (uint8_t*)data->value;

    if(tmp_data == NULL) {
        return NULL;
    }

    len = *((uint64_t*)tmp_data);
    l_processed += sizeof(uint64_t);

    if(len == 0) {
        *processed = l_processed;
        return NULL;
    }

    if(total_len < len) {
        return NULL;
    }

    tmp_data += sizeof(uint64_t);
    total_len -= sizeof(uint64_t);

    data_t* res = memory_malloc(sizeof(data_t));

    if(res == NULL) {
        return NULL;
    }

    if(total_len < 1) {
        goto catch_error;
    }

    res->type = *tmp_data;

    tmp_data++;
    total_len--;
    l_processed++;

    data_t td_name = {DATA_TYPE_INT8_ARRAY, total_len, NULL, tmp_data};
    uint64_t name_len = 0;
    res->name = data_bson_deserialize_with_processed(&td_name, &name_len);

    if(res->name && total_len < res->name->length) {
        memory_free(res);

        return NULL;
    }

    tmp_data += name_len;
    total_len -= name_len;
    l_processed += name_len;

    if(res->type >= DATA_TYPE_STRING) {
        if(total_len < 8) {
            goto catch_error;
        }

        res->length = *((uint64_t*)tmp_data);

        tmp_data += sizeof(uint64_t);
        total_len -= sizeof(uint64_t);
        l_processed += sizeof(uint64_t);
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
        *processed = l_processed;

        return res;
    }

    if(res->type == DATA_TYPE_DATA) {
        data_t* data_items = memory_malloc(sizeof(data_t) * res->length);

        for(uint64_t i = 0; i < res->length; i++) {
            data_t td = {DATA_TYPE_INT8_ARRAY, total_len, NULL, tmp_data};
            uint64_t item_len = 0;
            data_t* data_item = data_bson_deserialize_with_processed(&td, &item_len);

            if(data_item == NULL) {
                memory_free(data_items);
                data_free(res);

                return NULL;
            }

            memory_memcopy(data_item, data_items + i, sizeof(data_t));

            memory_free(data_item);

            tmp_data += item_len;
            total_len -= item_len;
            l_processed += item_len;
        }

        res->value = data_items;

        *processed = l_processed;

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

    *processed = l_processed + ilen;

    return res;

catch_error:
    data_free(res);

    return NULL;
}

void data_free(data_t* data) {
    if(data->name) {
        data_free(data->name);
    }

    if(data->type >= DATA_TYPE_STRING) {
        if(data->type == DATA_TYPE_DATA && data->value) {
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
