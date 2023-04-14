/**
 * @file tosdb_record_search.64.c
 * @brief tosdb record interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb_internal.h>
#include <video.h>

typedef struct tosdb_record_search_iter_ctx_t {
    tosdb_record_t* record;
    set_t*          pks;
    iterator_t*     set_iter;
} tosdb_record_search_iter_ctx_t;


int8_t      tosdb_rsi_end_of_iterator(iterator_t* iter);
iterator_t* tosdb_rsi_next(iterator_t* iter);
int8_t      tosdb_rsi_destroy(iterator_t* iter);
const void* tosdb_rsi_get_item(iterator_t* iter);

int8_t tosdb_rsi_end_of_iterator(iterator_t* iter) {
    tosdb_record_search_iter_ctx_t* ctx = iter->metadata;

    return ctx->set_iter->end_of_iterator(ctx->set_iter);
}

iterator_t* tosdb_rsi_next(iterator_t* iter) {
    tosdb_record_search_iter_ctx_t* ctx = iter->metadata;

    ctx->set_iter = ctx->set_iter->next(ctx->set_iter);

    return iter;
}

int8_t tosdb_rsi_destroy(iterator_t* iter) {
    tosdb_record_search_iter_ctx_t* ctx = iter->metadata;

    while(ctx->set_iter->end_of_iterator(ctx->set_iter)) {
        void* item = (void*)ctx->set_iter->get_item(ctx->set_iter);

        memory_free(item);

        ctx->set_iter = ctx->set_iter->next(ctx->set_iter);
    }

    ctx->set_iter->destroy(ctx->set_iter);

    set_destroy(ctx->pks);

    memory_free(ctx);

    memory_free(iter);

    return 0;
}

const void* tosdb_rsi_get_item(iterator_t* iter) {
    tosdb_record_search_iter_ctx_t* ctx = iter->metadata;
    tosdb_record_context_t* r_ctx = ctx->record->context;
    tosdb_table_t* tbl = r_ctx->table;

    uint64_t pk_col_id = tbl->primary_column_id;
    data_type_t pk_col_type = tbl->primary_column_type;

    tosdb_memtable_index_item_t* idx_item = (tosdb_memtable_index_item_t*)ctx->set_iter->get_item(ctx->set_iter);

    tosdb_record_t* rec = tosdb_table_create_record(tbl);

    if(!rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record for search");
        memory_free(idx_item);

        return NULL;
    }

    void* key = idx_item->key;
    uint64_t key_length = idx_item->key_length;

    if(pk_col_type < DATA_TYPE_STRING) {
        key = (void*)idx_item->key_hash;

        switch(pk_col_type) {
        case DATA_TYPE_CHAR:
        case DATA_TYPE_INT8:
        case DATA_TYPE_BOOLEAN:
            key_length = 1;
            break;
        case DATA_TYPE_INT16:
            key_length = 2;
            break;
        case DATA_TYPE_INT32:
            key_length = 4;
            break;
        case DATA_TYPE_INT64:
            key_length = 8;
            break;
        default:
            break;
        }
    }


    if(!tosdb_record_set_data_with_colid(rec, pk_col_id, pk_col_type, key_length, key)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set record pk for search");
        memory_free(idx_item);
        rec->destroy(rec);

        return NULL;
    }

    memory_free(idx_item);

    if(!rec->get_record(rec)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get record for search");
        rec->destroy(rec);

        return NULL;
    }

    return rec;
}

iterator_t* tosdb_record_search(tosdb_record_t* record) {
    iterator_t* iter = memory_malloc(sizeof(iterator_t));

    if(!iter) {
        return NULL;
    }

    tosdb_record_search_iter_ctx_t* ctx = memory_malloc(sizeof(tosdb_record_search_iter_ctx_t));

    if(!ctx) {
        memory_free(iter);

        return NULL;
    }

    iter->metadata = ctx;
    iter->end_of_iterator = tosdb_rsi_end_of_iterator;
    iter->next = tosdb_rsi_next;
    iter->destroy = tosdb_rsi_destroy;
    iter->get_item = tosdb_rsi_get_item;

    set_t* results = set_create(tosdb_memtable_index_comparator);

    if(!results) {
        memory_free(ctx);
        memory_free(iter);

        return NULL;
    }

    if(!tosdb_memtable_search(record, results)) {
        set_destroy(results);
        memory_free(ctx);
        memory_free(iter);

        return NULL;
    }

    if(!tosdb_sstable_search(record, results)) {
        set_destroy(results);
        memory_free(ctx);
        memory_free(iter);

        return NULL;
    }

    ctx->pks = results;
    ctx->set_iter = set_create_iterator(ctx->pks);

    ctx->record = record;

    return iter;
}


