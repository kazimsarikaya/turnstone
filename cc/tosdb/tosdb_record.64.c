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
boolean_t tosdb_record_get_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, uint8_t** value);
boolean_t tosdb_record_set_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value);
boolean_t tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, void** value);
boolean_t tosdb_record_destroy(tosdb_record_t* record);

typedef struct tosdb_record_context_t {
    tosdb_table_t* table;
    map_t          columns;
} tosdb_record_context_t;

boolean_t tosdb_record_set_boolean(tosdb_record_t * record, const char_t* colname, const boolean_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_BOOLEAN, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_boolean (tosdb_record_t * record, const char_t* colname, boolean_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_char(tosdb_record_t * record, const char_t* colname, const char_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_CHAR, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_char(tosdb_record_t * record, const char_t* colname, char_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_int8(tosdb_record_t * record, const char_t* colname, const int8_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int8(tosdb_record_t * record, const char_t* colname, int8_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_uint8(tosdb_record_t * record, const char_t* colname, const uint8_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint8(tosdb_record_t * record, const char_t* colname, uint8_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_int16(tosdb_record_t * record, const char_t* colname, const int16_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT16, sizeof(boolean_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int16(tosdb_record_t * record, const char_t* colname, int16_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_uint16(tosdb_record_t * record, const char_t* colname, const uint16_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT16, sizeof(int16_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint16(tosdb_record_t * record, const char_t* colname, uint16_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_int32(tosdb_record_t * record, const char_t* colname, const int32_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT32, sizeof(int16_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_int32(tosdb_record_t * record, const char_t* colname, int32_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_uint32(tosdb_record_t * record, const char_t* colname, const uint32_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT32, sizeof(int32_t), (void*)(uint64_t)value);
}

boolean_t tosdb_record_get_uint32(tosdb_record_t * record, const char_t* colname, uint32_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_int64(tosdb_record_t * record, const char_t* colname, const int64_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT64, sizeof(int64_t), (void*)value);
}

boolean_t tosdb_record_get_int64(tosdb_record_t * record, const char_t* colname, int64_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_uint64(tosdb_record_t * record, const char_t* colname, const uint64_t value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT64, sizeof(uint64_t), (void*)value);
}

boolean_t tosdb_record_get_uint64(tosdb_record_t * record, const char_t* colname, uint64_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_string(tosdb_record_t * record, const char_t* colname, const char_t* value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_STRING, strlen(value), value);
}

boolean_t tosdb_record_get_string(tosdb_record_t * record, const char_t* colname, char_t** value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_float32(tosdb_record_t * record, const char_t* colname, const float32_t value) {
    uint64_t tmp = 0;
    memory_memcopy(&value, &tmp, sizeof(float32_t));
    return tosdb_record_set_data(record, colname, DATA_TYPE_BOOLEAN, sizeof(boolean_t), (void*)tmp);
}

boolean_t tosdb_record_get_float32(tosdb_record_t * record, const char_t* colname, float32_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_float64(tosdb_record_t * record, const char_t* colname, const float64_t value) {
    uint64_t tmp = 0;
    memory_memcopy(&value, &tmp, sizeof(float64_t));
    return tosdb_record_set_data(record, colname, DATA_TYPE_BOOLEAN, sizeof(boolean_t), (void*)tmp);
}

boolean_t tosdb_record_get_float64(tosdb_record_t * record, const char_t* colname, float64_t* value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_set_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, const uint8_t* value) {
    return tosdb_record_set_data(record, colname, DATA_TYPE_INT8_ARRAY, len, value);
}

boolean_t tosdb_record_get_bytearray(tosdb_record_t * record, const char_t* colname, uint64_t len, uint8_t** value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(len);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
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

    data_t* name = memory_malloc(sizeof(data_t));

    if(!name) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create required item");

        return false;
    }

    name->type = DATA_TYPE_INT64;
    name->value = (void*)col->id;

    data_t* col_value = memory_malloc(sizeof(data_t));

    if(!col_value) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create required item");
        memory_free(name);

        return false;
    }

    col_value->name = name;
    col_value->type = type;
    col_value->length = len;
    col_value->value = (void*)value;

    map_insert(ctx->columns, (void*)col->id, col_value);

    return true;
}

boolean_t tosdb_record_get_data(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, void** value) {
    UNUSED(record);
    UNUSED(colname);
    UNUSED(type);
    UNUSED(len);
    UNUSED(value);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_record_destroy(tosdb_record_t * record){
    if(!record) {
        return true;
    }

    tosdb_record_context_t* ctx = record->context;

    iterator_t* iter = map_create_iterator(ctx->columns);

    while(iter->end_of_iterator(iter) != 0) {
        data_t* d = (data_t*)iter->get_item(iter);
        memory_free(d->name);
        memory_free(d);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    map_destroy(ctx->columns);
    memory_free(record->context);

    memory_free(record);

    return false;
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

    return rec;
}
