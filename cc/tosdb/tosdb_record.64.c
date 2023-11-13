/**
 * @file tosdb_record.64.c
 * @brief tosdb record interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <logging.h>
#include <strings.h>
#include <iterator.h>
#include <xxhash.h>
#include <set.h>
#include <random.h>
#include <time.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_record_set_boolean(tosdb_record_t * record, const char_t* colname, const boolean_t value);
boolean_t tosdb_record_get_boolean(tosdb_record_t * record, const char_t* colname, boolean_t* value);
boolean_t tosdb_record_set_char(tosdb_record_t * record, const char_t* colname, const char_t value);
boolean_t tosdb_record_get_char(tosdb_record_t * record, const char_t* colname, char_t* value);
boolean_t tosdb_record_set_int8(tosdb_record_t * record, const char_t* colname, const int8_t value);
boolean_t tosdb_record_get_int8(tosdb_record_t * record, const char_t* colname, int8_t* value);
boolean_t tosdb_record_set_uint8(tosdb_record_t * record, const char_t* colname, const uint8_t value);
boolean_t tosdb_record_get_uint8(tosdb_record_t * record, const char_t* colname, uint8_t* value);
boolean_t tosdb_record_set_int16(tosdb_record_t * record, const char_t* colname, const int16_t value);
boolean_t tosdb_record_get_int16(tosdb_record_t * record, const char_t* colname, int16_t* value);
boolean_t tosdb_record_set_uint16(tosdb_record_t * record, const char_t* colname, const uint16_t value);
boolean_t tosdb_record_get_uint16(tosdb_record_t * record, const char_t* colname, uint16_t* value);
boolean_t tosdb_record_set_int32(tosdb_record_t * record, const char_t* colname, const int32_t value);
boolean_t tosdb_record_get_int32(tosdb_record_t * record, const char_t* colname, int32_t* value);
boolean_t tosdb_record_set_uint32(tosdb_record_t * record, const char_t* colname, const uint32_t value);
boolean_t tosdb_record_get_uint32(tosdb_record_t * record, const char_t* colname, uint32_t* value);
boolean_t tosdb_record_set_int64(tosdb_record_t * record, const char_t* colname, const int64_t value);
boolean_t tosdb_record_get_int64(tosdb_record_t * record, const char_t* colname, int64_t* value);
boolean_t tosdb_record_set_uint64(tosdb_record_t * record, const char_t* colname, const uint64_t value);
boolean_t tosdb_record_get_uint64(tosdb_record_t * record, const char_t* colname, uint64_t* value);
boolean_t tosdb_record_set_string(tosdb_record_t * record, const char_t* colname, const char_t* value);
boolean_t tosdb_record_get_string(tosdb_record_t * record, const char_t* colname, char_t** value);
boolean_t tosdb_record_set_float32(tosdb_record_t * record, const char_t* colname, const float32_t value);
boolean_t tosdb_record_get_float32(tosdb_record_t * record, const char_t* colname, float32_t* value);
boolean_t tosdb_record_set_float64(tosdb_record_t * record, const char_t* colname, const float64_t value);
boolean_t tosdb_record_get_float64(tosdb_record_t * record, const char_t* colname, float64_t* value);
boolean_t tosdb_record_set_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, const uint8_t* value);
boolean_t tosdb_record_get_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t* len, uint8_t** value);
boolean_t tosdb_record_set_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value);
boolean_t tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t* len, void** value);
boolean_t tosdb_record_upsert(tosdb_record_t* record);
boolean_t tosdb_record_delete(tosdb_record_t* record);
boolean_t tosdb_record_get(tosdb_record_t* record);
boolean_t tosdb_record_destroy(tosdb_record_t* record);
boolean_t tosdb_record_is_deleted(tosdb_record_t* record);
uint64_t  tosdb_record_get_index_id(tosdb_record_t* record, uint64_t colid);

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
    return tosdb_record_get_data(record, colname, DATA_TYPE_INT8_ARRAY, len, (void**)value);
}

boolean_t tosdb_record_set_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value) {
    if(!record || !record->context || !strlen(colname)) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    const tosdb_column_t* col = hashmap_get(ctx->table->columns, colname);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s is not exists at table %s", colname, ctx->table->name);

        return false;
    }

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
        uint64_t null_byte = (type == DATA_TYPE_STRING)?1:0;

        l_value = memory_malloc(len + null_byte);

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

        tosdb_record_key_t* old_r_key = (tosdb_record_key_t*)hashmap_put(ctx->keys, (void*)idx_id, (void*)r_key);

        if(old_r_key) {
            if(old_r_key->key_length) {
                memory_free(old_r_key->key);
            }

            memory_free(old_r_key);
        }
    }

    data_t* old_col_value = (data_t*)hashmap_put(ctx->columns, (void*)col_id, col_value);

    if(old_col_value) {
        data_free(old_col_value);
    }

    return true;
}
#pragma GCC diagnostic pop

boolean_t tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t* len, void** value) {
    if(!record || !record->context || !strlen(colname)) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colname is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    const tosdb_column_t* col = hashmap_get(ctx->table->columns, colname);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s is not exists at table %s", colname, ctx->table->name);

        return false;
    }

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

    const data_t* d = hashmap_get(ctx->columns, (void*)col_id);

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
    if(!record) {
        return false;
    }

    if(tosdb_memtable_is_deleted(record)) {
        return true;
    }

    return tosdb_memtable_upsert(record, true);
}

boolean_t tosdb_record_get(tosdb_record_t* record) {
    if(!record) {
        return false;
    }

    if(tosdb_memtable_get(record)) {
        if(tosdb_record_is_deleted(record)) {
            return false;
        }

        return true;
    }

    if(tosdb_sstable_get(record)) {
        if(tosdb_record_is_deleted(record)) {
            return false;
        }

        return true;
    }

    return false;
}

boolean_t tosdb_record_search_set_destroy_cb(void * item) {
    if(!item) {
        return true;
    }

    tosdb_record_t* rec = item;

    return rec->destroy(rec);
}

static boolean_t tosdb_record_search_set_destroy_mii_cb(void * item) {
    if(!item) {
        return true;
    }

    memory_free(item);

    return true;
}

static boolean_t tosdb_record_compare_values(data_type_t type, uint64_t item1_len, void* item1, uint64_t item2_len, void* item2) {
    if(type < DATA_TYPE_STRING) {
        uint64_t tmp1 = (uint64_t)item1;
        uint64_t tmp2 = (uint64_t)item2;

        if(tmp1 != tmp2) {
            return false;
        }
    } else if(type == DATA_TYPE_STRING || type == DATA_TYPE_INT8_ARRAY) {
        if(item1_len != item2_len) {
            return false;
        }

        if(memory_memcompare(item1, item2, item1_len)) {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

linkedlist_t* tosdb_record_search(tosdb_record_t* record) {
    if(!record || !record->context) {
        return NULL;
    }

    tosdb_record_context_t* ctx = record->context;

    if(hashmap_size(ctx->keys) != 1) {
        PRINTLOG(TOSDB, LOG_ERROR, "record search supports only one key");

        return NULL;
    }

    iterator_t* iter = hashmap_iterator_create(ctx->keys);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get key");

        return NULL;
    }

    const tosdb_record_key_t* r_key = iter->get_item(iter);

    iter->destroy(iter);

    if(!r_key) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get key");

        return NULL;
    }

    const tosdb_index_t* index = hashmap_get(ctx->table->indexes, (void*)r_key->index_id);

    if(!index) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get index");

        return NULL;
    }

    const tosdb_column_t* col = tosdb_table_get_column_by_index_id(ctx->table, r_key->index_id);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get column");

        return NULL;
    }

    uint8_t* search_key = NULL;
    uint64_t search_key_len = 0;

    if(!tosdb_record_get_data_with_colid(record, col->id, col->type, &search_key_len, (void**)&search_key)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get data");

        return NULL;
    }

    if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %s will be searched on table %s", search_key, ctx->table->name);
    } else {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %lli will be searched on table %s", (int64_t)search_key, ctx->table->name);
    }


    set_t* results = set_create(tosdb_memtable_record_id_comparator);

    if(!results) {
        memory_free(search_key);

        return NULL;
    }

    if(!tosdb_memtable_search(record, results)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot search at memtable");
        set_destroy_with_callback(results, tosdb_record_search_set_destroy_mii_cb);
        memory_free(search_key);

        return NULL;
    }

    if(!tosdb_sstable_search(record, results)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot search at sstables");
        set_destroy_with_callback(results, tosdb_record_search_set_destroy_mii_cb);
        memory_free(search_key);

        return NULL;
    }

    if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %s searched, prob res size %lli", search_key, set_size(results));
    } else {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %lli searched prob res size %lli", (int64_t)search_key, set_size(results));
    }

    iterator_t* f_iter = set_create_iterator(results);

    if(!f_iter) {
        set_destroy_with_callback(results, tosdb_record_search_set_destroy_mii_cb);
        memory_free(search_key);

        return NULL;
    }

    tosdb_record_context_t* r_ctx = record->context;

    linkedlist_t* recs = linkedlist_create_list();

    boolean_t dont_add = false;

    if(!recs) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create results list");
        dont_add = true;
    }

    while(f_iter->end_of_iterator(f_iter) != 0) {
        tosdb_memtable_index_item_t* item = (tosdb_memtable_index_item_t*)f_iter->get_item(f_iter);

        if(!dont_add) {
            tosdb_record_t* rec = tosdb_table_create_record(r_ctx->table);

            uint64_t len = item->key_length;
            void* value = item->key;

            if(len == 0) {
                switch(r_ctx->table->primary_column_type) {
                case DATA_TYPE_CHAR:
                case DATA_TYPE_INT8:
                case DATA_TYPE_BOOLEAN:
                    len = 1;
                    break;
                case DATA_TYPE_INT16:
                    len = 2;
                    break;
                case DATA_TYPE_INT32:
                    len = 4;
                    break;
                case DATA_TYPE_INT64:
                    len = 8;
                    break;
                default:
                    break;
                }

                value = (void*)item->key_hash;
            }

            if(rec) {
                if(!tosdb_record_set_data_with_colid(rec,
                                                     r_ctx->table->primary_column_id,
                                                     r_ctx->table->primary_column_type,
                                                     len,
                                                     value)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot set pk");
                    dont_add = true;
                    rec->destroy(rec);
                } else if(rec->get_record(rec)) {
                    uint8_t* res_key_data = NULL;
                    uint64_t res_key_len = 0;

                    if(item->is_deleted) {
                        PRINTLOG(TOSDB, LOG_INFO, "record is deleted rec id %llx", (uint64_t)item->record_id);
                    }

                    if(!tosdb_record_get_data_with_colid(rec, col->id, col->type, &res_key_len, (void**)&res_key_data)) {
                        PRINTLOG(TOSDB, LOG_ERROR, "cannot get result key data");
                        dont_add = true;
                        rec->destroy(rec);
                    } else {
                        if(res_key_len != search_key_len) {
                            PRINTLOG(TOSDB, LOG_TRACE, "search key len %lli != result key len %lli", search_key_len, res_key_len);

                            if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
                                PRINTLOG(TOSDB, LOG_TRACE, "search key %s != result key %s", search_key, res_key_data);
                            } else {
                                PRINTLOG(TOSDB, LOG_TRACE, "search key %lli != result key %lli", (int64_t)search_key, (int64_t)res_key_data);
                            }

                            rec->destroy(rec);
                        } else {
                            if(!tosdb_record_compare_values(col->type, search_key_len, search_key, res_key_len, res_key_data)) {
                                if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
                                    PRINTLOG(TOSDB, LOG_TRACE, "search key %s != result key %s", search_key, res_key_data);
                                } else {
                                    PRINTLOG(TOSDB, LOG_TRACE, "search key %lli != result key %lli", (int64_t)search_key, (int64_t)res_key_data);
                                }

                                rec->destroy(rec);
                            } else {
                                if(linkedlist_list_insert(recs, rec) == -1ULL) {
                                    PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record to list");
                                    dont_add = true;
                                    rec->destroy(rec);
                                }
                            }
                        }

                        if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
                            memory_free(res_key_data);
                        }
                    }

                } else if(tosdb_record_is_deleted(rec)) {
                    if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
                        PRINTLOG(TOSDB, LOG_TRACE, "searced key %s record key: %s is already deleted", search_key, item->key);
                    } else {
                        PRINTLOG(TOSDB, LOG_TRACE, "searched key %lli record key: %lli is already deleted", (int64_t)search_key, item->key_hash);
                    }

                    rec->destroy(rec);
                } else {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot get record");
                    dont_add = true;

                    if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
                        PRINTLOG(TOSDB, LOG_ERROR, "searced key %s record key: %s", search_key, item->key);
                    } else {
                        PRINTLOG(TOSDB, LOG_ERROR, "searched key %lli record key: %lli", (int64_t)search_key, item->key_hash);
                    }

                    rec->destroy(rec);
                }
            } else {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
                dont_add = true;
            }
        }

        memory_free(item);

        f_iter = f_iter->next(f_iter);
    }

    f_iter->destroy(f_iter);

    set_destroy(results);

    if(col->type == DATA_TYPE_STRING || col->type == DATA_TYPE_INT8_ARRAY) {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %s ended", search_key);
        memory_free(search_key);
    } else {
        PRINTLOG(TOSDB, LOG_TRACE, "search key %lli ended", (int64_t)search_key);
    }

    return recs;
}


uint64_t tosdb_record_get_index_id(tosdb_record_t* record, uint64_t colid) {
    if(!record || !record->context || !colid) {
        PRINTLOG(TOSDB, LOG_ERROR, "record or colid failed");

        return 0;
    }

    tosdb_record_context_t* ctx = record->context;

    const tosdb_index_t* idx = hashmap_get(ctx->table->index_column_map, (void*)colid);

    if(!idx) {
        return 0;
    }

    return idx->id;
}

boolean_t tosdb_record_destroy(tosdb_record_t * record){
    if(!record) {
        return true;
    }

    tosdb_record_context_t* ctx = record->context;

    iterator_t* iter = hashmap_iterator_create(ctx->columns);

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

    hashmap_destroy(ctx->columns);

    iter = hashmap_iterator_create(ctx->keys);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_key_t* key = (tosdb_record_key_t*)iter->get_item(iter);
        memory_free(key->key);
        memory_free(key);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    hashmap_destroy(ctx->keys);

    memory_free(record->context);

    memory_free(record);

    return true;
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
    s_data.length = hashmap_size(ctx->columns);

    data_t* s_items = memory_malloc(sizeof(data_t) * s_data.length);

    if(!s_items) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record item list");

        return NULL;
    }

    s_data.value = s_items;

    uint64_t idx = 0;

    iterator_t* iter = hashmap_iterator_create(ctx->columns);

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

boolean_t tosdb_record_is_deleted(tosdb_record_t* record) {
    if(!record || !record->context) {
        PRINTLOG(TOSDB, LOG_ERROR, "record is null");

        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    return ctx->is_deleted;
}

tosdb_record_t* tosdb_table_create_record(tosdb_table_t* tbl) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return NULL;
    }

    if(!tbl->is_open || tbl->is_deleted) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed(%i) or deleted(%i)", !tbl->is_open, tbl->is_deleted);

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
    ctx->columns = hashmap_integer(128);

    if(!ctx->columns) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record context column map");
        memory_free(ctx);
        memory_free(rec);

        return NULL;
    }

    ctx->keys = hashmap_integer(128);

    if(!ctx->keys) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record context keys map");
        hashmap_destroy(ctx->columns);
        memory_free(ctx);
        memory_free(rec);

        return NULL;
    }

    uint128_t rec_id = time_ns(NULL);
    rec_id <<= 64;
    rec_id |= rand64();

    xxhash64_context_t* hash_ctx = xxhash64_init(0);
    xxhash64_update(hash_ctx, ctx->table->db->name, strlen(ctx->table->db->name));
    xxhash64_update(hash_ctx, ctx->table->name, strlen(ctx->table->name));
    xxhash64_update(hash_ctx, &rec_id, sizeof(uint128_t));
    uint64_t hash = xxhash64_final(hash_ctx);

    rec_id |= hash;

    ctx->record_id = rec_id;

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
    rec->is_deleted = tosdb_record_is_deleted;

    return rec;
}
