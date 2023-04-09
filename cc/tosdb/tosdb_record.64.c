/**
 * @file tosdb_record.64.c
 * @brief tosdb record interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <strings.h>
#include <iterator.h>
#include <xxhash.h>

boolean_t   tosdb_record_set_boolean(tosdb_record_t * record, const char_t* colname, const boolean_t value);
boolean_t   tosdb_record_get_boolean(tosdb_record_t * record, const char_t* colname, boolean_t* value);
boolean_t   tosdb_record_set_char(tosdb_record_t * record, const char_t* colname, const char_t value);
boolean_t   tosdb_record_get_char(tosdb_record_t * record, const char_t* colname, char_t* value);
boolean_t   tosdb_record_set_int8(tosdb_record_t * record, const char_t* colname, const int8_t value);
boolean_t   tosdb_record_get_int8(tosdb_record_t * record, const char_t* colname, int8_t* value);
boolean_t   tosdb_record_set_uint8(tosdb_record_t * record, const char_t* colname, const uint8_t value);
boolean_t   tosdb_record_get_uint8(tosdb_record_t * record, const char_t* colname, uint8_t* value);
boolean_t   tosdb_record_set_int16(tosdb_record_t * record, const char_t* colname, const int16_t value);
boolean_t   tosdb_record_get_int16(tosdb_record_t * record, const char_t* colname, int16_t* value);
boolean_t   tosdb_record_set_uint16(tosdb_record_t * record, const char_t* colname, const uint16_t value);
boolean_t   tosdb_record_get_uint16(tosdb_record_t * record, const char_t* colname, uint16_t* value);
boolean_t   tosdb_record_set_int32(tosdb_record_t * record, const char_t* colname, const int32_t value);
boolean_t   tosdb_record_get_int32(tosdb_record_t * record, const char_t* colname, int32_t* value);
boolean_t   tosdb_record_set_uint32(tosdb_record_t * record, const char_t* colname, const uint32_t value);
boolean_t   tosdb_record_get_uint32(tosdb_record_t * record, const char_t* colname, uint32_t* value);
boolean_t   tosdb_record_set_int64(tosdb_record_t * record, const char_t* colname, const int64_t value);
boolean_t   tosdb_record_get_int64(tosdb_record_t * record, const char_t* colname, int64_t* value);
boolean_t   tosdb_record_set_uint64(tosdb_record_t * record, const char_t* colname, const uint64_t value);
boolean_t   tosdb_record_get_uint64(tosdb_record_t * record, const char_t* colname, uint64_t* value);
boolean_t   tosdb_record_set_string(tosdb_record_t * record, const char_t* colname, const char_t* value);
boolean_t   tosdb_record_get_string(tosdb_record_t * record, const char_t* colname, char_t** value);
boolean_t   tosdb_record_set_float32(tosdb_record_t * record, const char_t* colname, const float32_t value);
boolean_t   tosdb_record_get_float32(tosdb_record_t * record, const char_t* colname, float32_t* value);
boolean_t   tosdb_record_set_float64(tosdb_record_t * record, const char_t* colname, const float64_t value);
boolean_t   tosdb_record_get_float64(tosdb_record_t * record, const char_t* colname, float64_t* value);
boolean_t   tosdb_record_set_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, const uint8_t* value);
boolean_t   tosdb_record_get_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t* len, uint8_t** value);
boolean_t   tosdb_record_set_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value);
boolean_t   tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t* len, void** value);
boolean_t   tosdb_record_upsert(tosdb_record_t* record);
boolean_t   tosdb_record_delete(tosdb_record_t* record);
boolean_t   tosdb_record_get(tosdb_record_t* record);
iterator_t* tosdb_record_search(tosdb_record_t* record);
boolean_t   tosdb_record_destroy(tosdb_record_t* record);

uint64_t tosdb_record_get_index_id(tosdb_record_t* record, uint64_t colid);

boolean_t tosdb_record_set_boolean(tosdb_record_t * record, const char_t* colname, const boolean_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_BOOLEAN, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_boolean (tosdb_record_t * record, const char_t* colname, boolean_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_BOOLEAN, NULL, (void**)value);
}

boolean_t tosdb_record_set_char(tosdb_record_t * record, const char_t* colname, const char_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_CHAR, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_char(tosdb_record_t * record, const char_t* colname, char_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_CHAR, NULL, (void**)value);
}

boolean_t tosdb_record_set_int8(tosdb_record_t * record, const char_t* colname, const int8_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int8(tosdb_record_t * record, const char_t* colname, int8_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT8, NULL, (void**)value);
}

boolean_t tosdb_record_set_uint8(tosdb_record_t * record, const char_t* colname, const uint8_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint8(tosdb_record_t * record, const char_t* colname, uint8_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT8, NULL, (void**)value);
}

boolean_t tosdb_record_set_int16(tosdb_record_t * record, const char_t* colname, const int16_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT16, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int16(tosdb_record_t * record, const char_t* colname, int16_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT16, NULL, (void**)value);
}

boolean_t tosdb_record_set_uint16(tosdb_record_t * record, const char_t* colname, const uint16_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT16, sizeof(int16_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint16(tosdb_record_t * record, const char_t* colname, uint16_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT16, NULL, (void**)value);
}

boolean_t tosdb_record_set_int32(tosdb_record_t * record, const char_t* colname, const int32_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT32, sizeof(int16_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int32(tosdb_record_t * record, const char_t* colname, int32_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT32, NULL, (void**)value);
}

boolean_t tosdb_record_set_uint32(tosdb_record_t * record, const char_t* colname, const uint32_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT32, sizeof(int32_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint32(tosdb_record_t * record, const char_t* colname, uint32_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT32, NULL, (void**)value);
}

boolean_t tosdb_record_set_int64(tosdb_record_t * record, const char_t* colname, const int64_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT64, sizeof(int64_t), (void*)value);
}

boolean_t tosdb_record_get_int64(tosdb_record_t * record, const char_t* colname, int64_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT64, NULL, (void**)value);
}

boolean_t tosdb_record_set_uint64(tosdb_record_t * record, const char_t* colname, const uint64_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT64, sizeof(uint64_t), (void*)value);
}

boolean_t tosdb_record_get_uint64(tosdb_record_t * record, const char_t* colname, uint64_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT64, NULL, (void**)value);
}

boolean_t tosdb_record_set_string(tosdb_record_t * record, const char_t* colname, const char_t* value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_STRING, strlen(value), value);
}

boolean_t tosdb_record_get_string(tosdb_record_t * record, const char_t* colname, char_t** value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_STRING, NULL, (void**)value);
}

boolean_t tosdb_record_set_float32(tosdb_record_t * record, const char_t* colname, const float32_t value) {
    uint64_t tmp = 0;
    memory_memcopy(&value, &tmp, sizeof(float32_t));
    return tosdb_record_set_data(record, colname, DATA_TYPE_FLOAT32, sizeof(boolean_t), (void*)tmp);
}

boolean_t tosdb_record_get_float32(tosdb_record_t * record, const char_t* colname, float32_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_FLOAT32, NULL, (void**)value);
}

boolean_t tosdb_record_set_float64(tosdb_record_t * record, const char_t* colname, const float64_t value) {
    uint64_t tmp = 0;
    memory_memcopy(&value, &tmp, sizeof(float64_t));
    return tosdb_record_set_data(record, colname, DATA_TYPE_FLOAT64, sizeof(boolean_t), (void*)tmp);
}

boolean_t tosdb_record_get_float64(tosdb_record_t * record, const char_t* colname, float64_t* value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_FLOAT64, NULL, (void**)value);
}

boolean_t tosdb_record_set_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, const uint8_t* value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8_ARRAY, len, value);
}

boolean_t tosdb_record_get_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t* len, uint8_t** value) {
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT8, len, (void**)value);
}

boolean_t tosdb_record_set_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value) {
    if(!record || !record->context || !strlen(colname)) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    const tosdb_column_t* col = map_get(ctx->table->columns, colname);

    if(col->type != type) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s type mismatch for table %s", colname, ctx->table->name);

        return false;
    }

    uint64_t col_id = col->id;

    return tosdb_record_set_data_with_colid(record, col_id, type, len, value);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_record_set_data_with_colid(tosdb_record_t * record, const uint64_t col_id, data_type_t type, uint64_t len, const void* value) {
    if(!record || !record->context || !col_id) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    data_t* name = memory_malloc(sizeof(data_t));

    if(!name) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create required item");

        return false;
    }

    name->type = DATA_TYPE_INT64;
    name->value = (void*)col_id;

    data_t* col_value = memory_malloc(sizeof(data_t));

    if(!col_value) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create required item");
        memory_free(name);

        return false;
    }

    void* l_value = NULL;

    if(type == DATA_TYPE_STRING || type == DATA_TYPE_INT8_ARRAY) {
        l_value = memory_malloc(len);

        if(!l_value) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create required item");
            memory_free(col_value);
            memory_free(name);

            return false;
        }

        memory_memcopy(value, l_value, len);
        col_value->value = (void*)l_value;
    } else {
        col_value->value = (void*)value;
    }

    col_value->name = name;
    col_value->type = type;
    col_value->length = len;

    uint64_t idx_id = tosdb_record_get_index_id(record, col_id);

    if(idx_id) {
        uint64_t key_hash = 0;

        if(type < DATA_TYPE_STRING) {
            key_hash = (uint64_t)value;
            len = 0;
        } else {
            key_hash = xxhash64_hash(l_value, len);
        }

        tosdb_record_key_t* r_key = memory_malloc(sizeof(tosdb_record_key_t));

        if(!r_key) {
            memory_free(name);
            memory_free(col_value);

            return false;
        }

        r_key->index_id = idx_id;
        r_key->key_hash = key_hash;
        r_key->key_length = len;
        r_key->key = (uint8_t*)l_value;

        map_insert(ctx->keys, (void*)idx_id, (void*)r_key);
    } else {
        if(type < DATA_TYPE_STRING) {
            memory_free(l_value);
        }
    }

    map_insert(ctx->columns, (void*)col_id, col_value);

    return true;
}
#pragma GCC diagnostic pop

boolean_t tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t* len, void** value) {
    if(!record || !record->context || !strlen(colname)) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    const tosdb_column_t* col = map_get(ctx->table->columns, colname);

    if(col->type != type) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s type mismatch for table %s", colname, ctx->table->name);

        return false;
    }

    uint64_t col_id = col->id;

    return tosdb_record_get_data_with_colid(record, col_id, type, len, value);
}

boolean_t tosdb_record_get_data_with_colid(tosdb_record_t * record, const uint64_t col_id, data_type_t type, uint64_t* len, void** value) {
    if(!record || !record->context || !col_id) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    if(!value) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    const data_t* d = map_get(ctx->columns, (void*)col_id);

    if(!d || d->type != type) {
        return false;
    }

    if(type < DATA_TYPE_STRING) {
        uint64_t tmp = (uint64_t)d->value;
        memory_memcopy(&tmp, value, d->length);
    } else if(type == DATA_TYPE_STRING) {
        *value = memory_malloc(d->length + 1);

        if(!value) {
            return false;
        }

        memory_memcopy(d->value, *value, d->length);
    } else if(type == DATA_TYPE_INT8_ARRAY) {
        *value = memory_malloc(d->length);

        if(!value) {
            return false;
        }

        memory_memcopy(d->value, *value, d->length);
    } else {
        return false;
    }

    if(len) {
        *len = d->length;
    }

    return true;
}

boolean_t tosdb_record_upsert(tosdb_record_t* record) {
    return tosdb_memtable_upsert(record, false);
}

boolean_t tosdb_record_delete(tosdb_record_t* record) {
    return tosdb_memtable_upsert(record, true);
}

boolean_t tosdb_record_get(tosdb_record_t* record) {
    if(!record) {
        return false;
    }

    if(tosdb_memtable_get(record)) {
        return true;
    }

    return tosdb_sstable_get(record);
}

iterator_t* tosdb_record_search(tosdb_record_t* record) {
    UNUSED(record);

    NOTIMPLEMENTEDLOG(TOSDB);

    return NULL;
}

uint64_t tosdb_record_get_index_id(tosdb_record_t* record, uint64_t colid) {
    if(!record || !record->context || !colid) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colid failed");

        return 0;
    }

    tosdb_record_context_t* ctx = record->context;

    uint64_t idx_id = 0;

    iterator_t* iter = map_create_iterator(ctx->table->indexes);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");

        return 0;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_index_t* idx = (tosdb_index_t*)iter->get_item(iter);


        if(idx->column_id == colid) {
            idx_id = idx->id;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return idx_id;
}

boolean_t tosdb_record_destroy(tosdb_record_t * record){
    if(!record) {
        return true;
    }

    tosdb_record_context_t* ctx = record->context;

    iterator_t* iter = map_create_iterator(ctx->columns);

    while(iter->end_of_iterator(iter) != 0) {
        data_t* d = (data_t*)iter->get_item(iter);

        if(d->type >= DATA_TYPE_STRING) {
            uint64_t col_id = (uint64_t)d->name->value;

            if(!tosdb_record_get_index_id(record, col_id)) {
                memory_free(d->value);
            }
        }

        memory_free(d->name);
        memory_free(d);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    map_destroy(ctx->columns);

    iter = map_create_iterator(ctx->keys);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_key_t* key = (tosdb_record_key_t*)iter->get_item(iter);
        memory_free(key->key);
        memory_free(key);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    map_destroy(ctx->keys);

    memory_free(record->context);

    memory_free(record);

    return false;
}

data_t* tosdb_record_serialize(tosdb_record_t* record) {
    if(!record || !record->context) {
        PRINTLOG(TOSDB, LOG_ERROR, "record is null");

        return NULL;
    }

    tosdb_record_context_t* ctx = record->context;

    if(!ctx->columns) {
        PRINTLOG(TOSDB, LOG_ERROR, "empty record");

        return NULL;
    }

    data_t s_data = {0};

    s_data.type = DATA_TYPE_DATA;
    s_data.length = map_size(ctx->columns);

    data_t* s_items = memory_malloc(sizeof(data_t) * s_data.length);

    if(!s_items) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record item list");

        return NULL;
    }

    s_data.value = s_items;

    uint64_t idx = 0;

    iterator_t* iter = map_create_iterator(ctx->columns);

    while(iter->end_of_iterator(iter) != 0) {
        data_t* d = (data_t*)iter->get_item(iter);

        s_items[idx].length = d->length;
        s_items[idx].name = d->name;
        s_items[idx].type = d->type;
        s_items[idx].value = d->value;

        idx++;
        iter = iter->next(iter);
    }

    iter->destroy(iter);

    data_t* res = data_bson_serialize(&s_data, DATA_SERIALIZE_WITH_ALL);

    memory_free(s_items);

    return res;
}

tosdb_record_t* tosdb_table_create_record(tosdb_table_t* tbl) {
    if(!tbl || !tbl->is_open || tbl->is_deleted) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null or closed or deleted");

        return NULL;
    }


    tosdb_record_t* rec = memory_malloc(sizeof(tosdb_record_t));

    if(!rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create rec at memory");

        return NULL;
    }

    tosdb_record_context_t* ctx = memory_malloc(sizeof(tosdb_record_context_t));

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record context");
        memory_free(rec);

        return NULL;
    }

    ctx->table = tbl;
    ctx->columns = map_integer();

    if(!ctx->columns) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record context column map");
        memory_free(ctx);
        memory_free(rec);

        return NULL;
    }

    ctx->keys = map_integer();

    if(!ctx->keys) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record context keys map");
        map_destroy(ctx->columns);
        memory_free(ctx);
        memory_free(rec);

        return NULL;
    }

    rec->context = ctx;
    rec->set_boolean = tosdb_record_set_boolean;
    rec->get_boolean = tosdb_record_get_boolean;
    rec->set_char = tosdb_record_set_char;
    rec->get_char = tosdb_record_get_char;
    rec->set_int8 = tosdb_record_set_int8;
    rec->get_int8 = tosdb_record_get_int8;
    rec->set_uint8 = tosdb_record_set_uint8;
    rec->get_uint8 = tosdb_record_get_uint8;
    rec->set_int16 = tosdb_record_set_int16;
    rec->get_int16 = tosdb_record_get_int16;
    rec->set_uint16 = tosdb_record_set_uint16;
    rec->get_uint16 = tosdb_record_get_uint16;
    rec->set_int32 = tosdb_record_set_int32;
    rec->get_int32 = tosdb_record_get_int32;
    rec->set_uint32 = tosdb_record_set_uint32;
    rec->get_uint32 = tosdb_record_get_uint32;
    rec->set_int64 = tosdb_record_set_int64;
    rec->get_int64 = tosdb_record_get_int64;
    rec->set_uint64 = tosdb_record_set_uint64;
    rec->get_uint64 = tosdb_record_get_uint64;
    rec->set_string = tosdb_record_set_string;
    rec->get_string = tosdb_record_get_string;
    rec->set_float32 = tosdb_record_set_float32;
    rec->get_float32 = tosdb_record_get_float32;
    rec->set_float64 = tosdb_record_set_float64;
    rec->get_float64 = tosdb_record_get_float64;
    rec->set_bytearray = tosdb_record_set_bytearray;
    rec->get_bytearray = tosdb_record_get_bytearray;
    rec->get_data = tosdb_record_get_data;
    rec->set_data = tosdb_record_set_data;
    rec->destroy = tosdb_record_destroy;
    rec->get_record = tosdb_record_get;
    rec->upsert_record = tosdb_record_upsert;
    rec->delete_record = tosdb_record_delete;
    rec->search_record = tosdb_record_search;

    return rec;
}
