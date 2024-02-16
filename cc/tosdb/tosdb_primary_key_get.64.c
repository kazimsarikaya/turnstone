/**
 * @file tosdb_primary_key_get.64.c
 * @brief tosdb primary key getter implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_cache.h>
#include <logging.h>
#include <strings.h>
#include <iterator.h>
#include <xxhash.h>
#include <set.h>
#include <compression.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_primary_key_memtable_get(const tosdb_table_t* tbl, uint64_t mt_id, const tosdb_memtable_index_t* idx, set_t* pks);
boolean_t tosdb_primary_key_sstable_get_on_list(const tosdb_table_t* tbl, list_t* st_list, set_t* pks, list_t* old_pks);
boolean_t tosdb_primary_key_sstable_get_on_index(const tosdb_table_t* tbl, tosdb_block_sstable_list_item_t* sli, set_t* pks, list_t* old_pks);

int8_t tosdb_record_primary_key_comparator(const void* item1, const void* item2) {
    tosdb_record_t* rec1 = (tosdb_record_t*)item1;
    tosdb_record_t* rec2 = (tosdb_record_t*)item2;

    tosdb_record_context_t* ctx1 = rec1->context;
    tosdb_record_context_t* ctx2 = rec2->context;


    if(ctx1->table->id < ctx2->table->id) {
        return -1;
    }

    if(ctx1->table->id > ctx2->table->id) {
        return 1;
    }

    const tosdb_record_key_t* key1 = hashmap_get(ctx1->keys, (void*)ctx1->table->primary_index_id);
    const tosdb_record_key_t* key2 = hashmap_get(ctx2->keys, (void*)ctx2->table->primary_index_id);

    if(key1->key_hash < key2->key_hash) {
        return -1;
    }

    if(key1->key_hash > key2->key_hash) {
        return 1;
    }

    uint64_t min = MIN(key1->key_length, key2->key_length);

    int8_t res = memory_memcompare(key1->key, key2->key, min);

    if(res) {
        return res;
    }

    if(key1->key_length == key2->key_length) {
        return res;
    }

    if(key1->key_length == min) {
        return -1;
    }

    return 1;
}


boolean_t tosdb_primary_key_memtable_get(const tosdb_table_t* tbl, uint64_t mt_id, const tosdb_memtable_index_t* idx, set_t* pks) {
    iterator_t* iter = idx->index->create_iterator(idx->index);

    if(!iter) {
        return false;
    }

    boolean_t error = false;

    while(iter->end_of_iterator(iter) != 0) {
        const tosdb_memtable_index_item_t* ii = iter->get_item(iter);

        tosdb_record_t* rec = tosdb_table_create_record((tosdb_table_t*)tbl);

        if(!rec) {
            error = true;
            break;
        }

        tosdb_record_context_t* ctx = rec->context;

        ctx->sstable_id = mt_id;

        uint64_t len = ii->key_length;
        void* value = (void*)ii->key;

        if(len == 0) {
            switch(tbl->primary_column_type) {
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

            value = (void*)ii->key_hash;
        }

        error = !tosdb_record_set_data_with_colid(rec, tbl->primary_column_id, tbl->primary_column_type, len, value);

        if(error) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set pk");
            rec->destroy(rec);

            break;
        }

        if(!set_append(pks, rec)) {
            if(!rec->destroy(rec)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot clean up");
                error = true;

                break;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    return !error;
}

boolean_t tosdb_primary_key_sstable_get_on_index(const tosdb_table_t* tbl, tosdb_block_sstable_list_item_t* sli, set_t* pks, list_t* old_pks) {

    uint64_t idx_loc = 0;
    uint64_t idx_size = 0;

    for(uint64_t i = 0; i < sli->index_count; i++) {
        if(tbl->primary_index_id == sli->indexes[i].index_id) {
            idx_loc = sli->indexes[i].index_location;
            idx_size = sli->indexes[i].index_size;

        }
    }

    if(!idx_loc || !idx_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find index %lli", tbl->primary_index_id);

        return false;
    }

    tosdb_cache_t* tdb_cache = tbl->db->tdb->cache;

    tosdb_cache_key_t cache_key = {0};

    cache_key.type = TOSDB_CACHE_ITEM_TYPE_INDEX_DATA;
    cache_key.database_id = tbl->db->id;
    cache_key.table_id = tbl->id;
    cache_key.index_id = tbl->primary_index_id;
    cache_key.level = sli->level;
    cache_key.sstable_id = sli->sstable_id;

    tosdb_cached_index_data_t* c_id = NULL;

    tosdb_memtable_index_item_t** st_idx_items = NULL;
    uint64_t record_count = 0;
    uint8_t* idx_data = NULL;

    if(tdb_cache) {
        c_id = (tosdb_cached_index_data_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_id) {
        st_idx_items = c_id->index_items;
        record_count = c_id->record_count;
    } else {
        tosdb_block_sstable_index_t* st_idx = (tosdb_block_sstable_index_t*)tosdb_block_read(tbl->db->tdb, idx_loc, idx_size);

        if(!st_idx) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read sstable index from backend");

            return false;
        }

        record_count = st_idx->record_count;

        idx_loc = st_idx->index_data_location;
        idx_size = st_idx->index_data_size;

        memory_free(st_idx);

        tosdb_block_sstable_index_data_t* b_sid = (tosdb_block_sstable_index_data_t*)tosdb_block_read(tbl->db->tdb, idx_loc, idx_size);

        if(!b_sid) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read index data");

            return false;

        }

        buffer_t* buf_idx_in = buffer_encapsulate(b_sid->data, b_sid->index_data_size);
        buffer_t* buf_idx_out = buffer_new_with_capacity(NULL, b_sid->index_data_unpacked_size);

        const compression_t* compression = tbl->db->tdb->compression;

        int8_t zc_res = compression->unpack(buf_idx_in, buf_idx_out);

        uint64_t zc = buffer_get_length(buf_idx_out);

        uint64_t index_data_unpacked_size = b_sid->index_data_unpacked_size;

        memory_free(b_sid);

        buffer_destroy(buf_idx_in);

        if(zc_res != 0 || zc != index_data_unpacked_size) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot zunpack idx");
            buffer_destroy(buf_idx_out);

            return false;
        }

        idx_data = buffer_get_all_bytes_and_destroy(buf_idx_out, NULL);

        st_idx_items = memory_malloc(sizeof(tosdb_memtable_index_item_t*) * record_count);

        if(!st_idx_items) {
            memory_free(idx_data);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");

            return false;
        }


        for(uint64_t i = 0; i < record_count; i++) {
            st_idx_items[i] = (tosdb_memtable_index_item_t*)idx_data;

            idx_data += sizeof(tosdb_memtable_index_item_t) + st_idx_items[i]->key_length;
        }

        if(tdb_cache) {
            c_id = memory_malloc(sizeof(tosdb_cached_index_data_t));

            if(!c_id) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate cached index data");
                memory_free(st_idx_items);
                memory_free(idx_data);

                return false;
            }

            memory_memcopy(&cache_key, c_id, sizeof(tosdb_cache_key_t));
            c_id->index_items = st_idx_items;
            c_id->record_count = record_count;
            c_id->valuelog_location = sli->valuelog_location;
            c_id->valuelog_size = sli->valuelog_size;
            c_id->cache_key.data_size = sizeof(tosdb_cached_index_data_t) + index_data_unpacked_size + sizeof(tosdb_memtable_index_item_t*) * record_count;

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_id);
        }
    }

    boolean_t error = false;

    for(uint64_t i = 0; i < record_count; i++) {
        const tosdb_memtable_index_item_t* ii = st_idx_items[i];

        tosdb_record_t* rec = tosdb_table_create_record((tosdb_table_t*)tbl);

        if(!rec) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
            error = true;

            break;
        }

        tosdb_record_context_t* ctx = rec->context;

        ctx->sstable_id = sli->sstable_id;
        ctx->level = sli->level;

        uint64_t len = ii->key_length;
        void* value = (void*)ii->key;

        if(len == 0) {
            switch(tbl->primary_column_type) {
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

            value = (void*)ii->key_hash;
        }

        error = !tosdb_record_set_data_with_colid(rec, tbl->primary_column_id, tbl->primary_column_type, len, value);

        if(error) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set pk");
            rec->destroy(rec);

            break;
        }

        if(!set_append(pks, rec)) {
            if(old_pks) {
                list_list_insert(old_pks, rec);
            } else {
                rec->destroy(rec);
            }
        }
    }


    if(!tdb_cache) {
        memory_free(st_idx_items[0]);
        memory_free(st_idx_items);
    }

    return !error;
}

boolean_t tosdb_primary_key_sstable_get_on_list(const tosdb_table_t* tbl, list_t* st_list, set_t* pks, list_t* old_pks) {
    boolean_t error = false;

    iterator_t* iter = list_iterator_create(st_list);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstables list items iterator");

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_block_sstable_list_item_t* sli = (tosdb_block_sstable_list_item_t*) iter->get_item(iter);

        if(tbl->primary_index_id <= sli->index_count) {
            if(!tosdb_primary_key_sstable_get_on_index(tbl, sli, pks, old_pks)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks on sstable %lli at level %lli", sli->sstable_id, sli->level);
                error = true;

                break;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return !error;
}

boolean_t tosdb_table_get_primary_keys_internal(const tosdb_table_t* tbl, set_t* pks, list_t* old_pks) {
    if(!tbl || !pks) {
        return false;
    }

    boolean_t error = false;

    uint64_t pk_idx_id = tbl->primary_index_id;

    if(tbl->memtables) {
        iterator_t* iter = list_iterator_create(tbl->memtables);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator for memtables");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            const tosdb_memtable_t* mt = iter->get_item(iter);

            const tosdb_memtable_index_t* idx = hashmap_get(mt->indexes, (void*)pk_idx_id);

            if(!mt->stli) {
                error = !tosdb_primary_key_memtable_get(tbl, mt->id, idx, pks);

                if(error) {
                    PRINTLOG(TOSDB, LOG_ERROR, "error at getting pks at memtables");

                    break;
                }
            }

            if(mt->stli && !tosdb_primary_key_sstable_get_on_index(tbl, mt->stli, pks, old_pks)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks on sstable %lli at level %lli", mt->stli->sstable_id, mt->stli->level);
                error = true;

                break;
            }

            iter = iter->next(iter);
        }

        iter->destroy(iter);
    }

    if(!error && tbl->sstable_list_items) {
        error = !tosdb_primary_key_sstable_get_on_list(tbl, tbl->sstable_list_items, pks, old_pks);
    }

    if(!error && tbl->sstable_levels) {
        for(uint64_t i = 1; i <= tbl->sstable_max_level; i++) {
            list_t* st_lvl_l = (list_t*)hashmap_get(tbl->sstable_levels, (void*)i);

            if(st_lvl_l) {
                if(!tosdb_primary_key_sstable_get_on_list(tbl, st_lvl_l, pks, old_pks)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks from sstable");
                    error = true;

                    break;
                }
            }
        }
    }

    return !error;
}
