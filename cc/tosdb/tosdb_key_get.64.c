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

int8_t tosdb_record_key_comparator(const void* item1, const void* item2) {
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

    if(ctx1->record_id < ctx2->record_id) {
        return -1;
    }

    if(ctx1->record_id > ctx2->record_id) {
        return 1;
    }

    return 0;
}


static boolean_t tosdb_key_memtable_get(const tosdb_table_t* tbl, const tosdb_memtable_index_t* idx, set_t* keys) {
    const tosdb_column_t* col = tosdb_table_get_column_by_index_id(tbl, idx->ti->id);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find column %lli", idx->ti->column_id);

        return false;
    }

    tosdb_index_type_t idx_type = idx->ti->type;

    iterator_t* iter = idx->index->create_iterator(idx->index);

    if(!iter) {
        return false;
    }

    boolean_t error = false;

    while(iter->end_of_iterator(iter) != 0) {
        const void* not_typed_ii = iter->get_item(iter);

        tosdb_record_t* rec = tosdb_table_create_record((tosdb_table_t*)tbl);

        if(!rec) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
            error = true;

            break;
        }

        tosdb_record_context_t* ctx = rec->context;

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            const tosdb_memtable_index_item_t* ii = not_typed_ii;
            ctx->sstable_id = ii->sstable_id;
            ctx->level = ii->level;
            ctx->record_id = ii->record_id;
            ctx->offset = ii->offset;
            ctx->length = ii->length;

        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            const tosdb_memtable_secondary_index_item_t* ii = not_typed_ii;
            ctx->sstable_id = ii->sstable_id;
            ctx->level = ii->level;
            ctx->record_id = ii->record_id;
            ctx->offset = ii->offset;
            ctx->length = ii->length;

        }

        if(!set_append(keys, rec)) {
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

static boolean_t tosdb_key_sstable_get_on_index(const tosdb_table_t* tbl, tosdb_block_sstable_list_item_t* sli,
                                                uint64_t key_index, set_t* keys, list_t* old_keys) {

    uint64_t idx_loc = 0;
    uint64_t idx_size = 0;

    const tosdb_index_t* idx = hashmap_get(tbl->indexes, (void*)key_index);

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find index %lli", key_index);

        return false;
    }

    tosdb_index_type_t idx_type = idx->type;

    for(uint64_t i = 0; i < sli->index_count; i++) {
        if(key_index == sli->indexes[i].index_id) {
            idx_loc = sli->indexes[i].index_location;
            idx_size = sli->indexes[i].index_size;

        }
    }

    if(!idx_loc || !idx_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find index %lli", key_index);

        return false;
    }

    tosdb_cache_t* tdb_cache = tbl->db->tdb->cache;

    tosdb_cache_key_t cache_key = {0};
    cache_key.type = TOSDB_CACHE_ITEM_TYPE_INDEX_DATA;
    cache_key.database_id = tbl->db->id;
    cache_key.table_id = tbl->id;
    cache_key.index_id = key_index;
    cache_key.level = sli->level;
    cache_key.sstable_id = sli->sstable_id;

    tosdb_cached_index_data_t* c_id = NULL;

    void** st_idx_items = NULL;
    uint64_t record_count = 0;
    uint8_t* idx_data = NULL;

    if(tdb_cache) {
        c_id = (void*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_id) {
        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            st_idx_items = (void**)c_id->index_items;
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            st_idx_items = (void**)c_id->secondary_index_items;
        }

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

        uint64_t st_idx_items_len = 0;

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            st_idx_items_len = sizeof(tosdb_memtable_index_item_t*) * record_count;
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            st_idx_items_len = sizeof(tosdb_memtable_secondary_index_item_t*) * record_count;
        }

        st_idx_items = memory_malloc(st_idx_items_len);

        if(!st_idx_items) {
            memory_free(idx_data);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");

            return false;
        }

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            for(uint64_t i = 0; i < record_count; i++) {
                st_idx_items[i] = idx_data;
                tosdb_memtable_index_item_t* ii = (tosdb_memtable_index_item_t*)st_idx_items[i];

                idx_data += sizeof(tosdb_memtable_index_item_t) + ii->key_length;
            }
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            for(uint64_t i = 0; i < record_count; i++) {
                st_idx_items[i] = idx_data;
                tosdb_memtable_secondary_index_item_t* ii = (tosdb_memtable_secondary_index_item_t*)st_idx_items[i];

                idx_data += sizeof(tosdb_memtable_secondary_index_item_t) + ii->secondary_key_length + ii->primary_key_length;
            }
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

            if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
                c_id->index_items = (tosdb_memtable_index_item_t**)st_idx_items;
            } else if(idx_type == TOSDB_INDEX_SECONDARY) {
                c_id->secondary_index_items = (tosdb_memtable_secondary_index_item_t**)st_idx_items;
            }

            c_id->record_count = record_count;
            c_id->valuelog_location = sli->valuelog_location;
            c_id->valuelog_size = sli->valuelog_size;
            c_id->cache_key.data_size = sizeof(tosdb_cached_index_data_t) + index_data_unpacked_size + st_idx_items_len;

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_id);
        }
    }

    boolean_t error = false;

    const tosdb_column_t* col = tosdb_table_get_column_by_index_id(tbl, idx->id);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find column %lli", idx->column_id);

        if(!tdb_cache) {
            memory_free(st_idx_items[0]);
            memory_free(st_idx_items);
        }

        return false;
    }

    for(uint64_t i = 0; i < record_count; i++) {

        tosdb_record_t* rec = tosdb_table_create_record((tosdb_table_t*)tbl);

        if(!rec) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
            error = true;

            break;
        }

        tosdb_record_context_t* ctx = rec->context;

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            const tosdb_memtable_index_item_t* ii = st_idx_items[i];
            ctx->sstable_id = sli->sstable_id;
            ctx->level = sli->level;
            ctx->record_id = ii->record_id;
            ctx->offset = ii->offset;
            ctx->length = ii->length;

        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            const tosdb_memtable_secondary_index_item_t* ii = st_idx_items[i];
            ctx->sstable_id = sli->sstable_id;
            ctx->level = sli->level;
            ctx->record_id = ii->record_id;
            ctx->offset = ii->offset;
            ctx->length = ii->length;

        }

        if(!set_append(keys, rec)) {
            if(old_keys) {
                list_list_insert(old_keys, rec);
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

static boolean_t tosdb_key_sstable_get_on_list(const tosdb_table_t* tbl, list_t* st_list, uint64_t key_index, set_t* keys, list_t* old_keys) {
    boolean_t error = false;

    for(uint64_t i = 0; i < list_size(st_list); i++) {
        tosdb_block_sstable_list_item_t* sli = (tosdb_block_sstable_list_item_t*)list_get_data_at_position(st_list, i);

        if(key_index <= sli->index_count) {
            if(!tosdb_key_sstable_get_on_index(tbl, sli, key_index, keys, old_keys)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks on sstable %lli at level %lli", sli->sstable_id, sli->level);
                error = true;

                break;
            }
        }
    }

    return !error;
}

boolean_t tosdb_table_get_keys_internal(const tosdb_table_t* tbl, uint64_t key_index, set_t* keys, list_t* old_keys) {
    if(!tbl || !keys) {
        return false;
    }

    boolean_t error = false;


    if(tbl->memtables) {
        for(uint64_t i = 0; i < list_size(tbl->memtables); i++) {
            const tosdb_memtable_t* mt = (tosdb_memtable_t*)list_get_data_at_position(tbl->memtables, i);

            const tosdb_memtable_index_t* idx = hashmap_get(mt->indexes, (void*)key_index);

            if(!mt->stli) {
                error = !tosdb_key_memtable_get(tbl, idx, keys);

                if(error) {
                    PRINTLOG(TOSDB, LOG_ERROR, "error at getting pks at memtables");

                    break;
                }
            }

            if(mt->stli && !tosdb_key_sstable_get_on_index(tbl, mt->stli, key_index, keys, old_keys)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks on sstable %lli at level %lli", mt->stli->sstable_id, mt->stli->level);
                error = true;

                break;
            }

        }
    }

    if(!error && tbl->sstable_list_items) {
        error = !tosdb_key_sstable_get_on_list(tbl, tbl->sstable_list_items, key_index, keys, old_keys);
    }

    if(!error && tbl->sstable_levels) {
        for(uint64_t i = 1; i <= tbl->sstable_max_level; i++) {
            list_t* st_lvl_l = (list_t*)hashmap_get(tbl->sstable_levels, (void*)i);

            if(st_lvl_l) {
                if(!tosdb_key_sstable_get_on_list(tbl, st_lvl_l, key_index, keys, old_keys)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot get pks from sstable");
                    error = true;

                    break;
                }
            }
        }
    }

    return !error;
}
