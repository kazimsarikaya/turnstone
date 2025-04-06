/**
 * @file tosdb_compaction.64.c
 * @brief tosdb compaction implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_cache.h>
#include <logging.h>
#include <stdbufs.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_compact(tosdb_t* tdb, tosdb_compaction_type_t type) {
    if(!tdb) {
        return false;
    }

    if(type == TOSDB_COMPACTION_TYPE_NONE) {
        return true;
    }

    if(type == TOSDB_COMPACTION_TYPE_MAJOR) {
        // if(!tosdb_compact(tdb, TOSDB_COMPACTION_TYPE_MINOR)) {
        // return false;
        // }
    }

    hashmap_t* dbs = tdb->databases;

    if(!dbs) {
        return true;
    }

    boolean_t error = false;

    iterator_t* db_iter = hashmap_iterator_create(dbs);

    if(!db_iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create db iterator");

        return false;
    }

    while(db_iter->end_of_iterator(db_iter)) {
        const tosdb_database_t* db = db_iter->get_item(db_iter);

        error = !tosdb_database_compact(db, type);

        db_iter = db_iter->next(db_iter);
    }

    db_iter->destroy(db_iter);

    return !error;
}

boolean_t tosdb_database_compact(const tosdb_database_t* db, tosdb_compaction_type_t type) {
    if(!db) {
        return false;
    }

    if(type == TOSDB_COMPACTION_TYPE_NONE) {
        return true;
    }

    hashmap_t* tbls = db->tables;

    if(!tbls) {
        return true;
    }

    boolean_t error = false;

    iterator_t* tbl_iter = hashmap_iterator_create(tbls);

    if(!tbl_iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create db iterator");

        return false;
    }

    while(tbl_iter->end_of_iterator(tbl_iter)) {
        const tosdb_table_t* tbl = tbl_iter->get_item(tbl_iter);

        error |= !tosdb_table_compact(tbl, type);

        tbl_iter = tbl_iter->next(tbl_iter);
    }

    tbl_iter->destroy(tbl_iter);

    return !error;
}

static int8_t tosdb_record_list_destroyer_cb(memory_heap_t* heap, void* data) {
    UNUSED(heap);

    if(!data) {
        return -1;
    }

    tosdb_record_t* rec = (tosdb_record_t*)data;

    rec->destroy(rec);

    return 0;
}

static const tosdb_block_sstable_list_item_t* tosdb_table_find_sstable_list_item(const tosdb_table_t* tbl, uint64_t level, uint64_t id) {
    if(!tbl) {
        return NULL;
    }

    if(level == 1) {
        list_t* tmp = tbl->sstable_list_items;

        for(uint64_t idx = 0; idx < list_size(tmp); idx++) {
            const tosdb_block_sstable_list_item_t* tmp_item = list_get_data_at_position(tmp, idx);

            if(tmp_item->sstable_id == id) {
                return tmp_item;
            }
        }
    }

    list_t* tmp = (list_t*)hashmap_get(tbl->sstable_levels, (void*)level);

    if(!tmp) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find sstable list item level: %lli id: %lli", level, id);
        return NULL;
    }

    for(uint64_t idx = 0; idx < list_size(tmp); idx++) {
        const tosdb_block_sstable_list_item_t* tmp_item = list_get_data_at_position(tmp, idx);

        if(tmp_item->sstable_id == id) {
            return tmp_item;
        }
    }

    return NULL;
}

static int8_t tosdb_compaction_existing_set_pk_comparator(const void* a, const void* b) {
    if(!a || !b) {
        return 0;
    }

    const tosdb_memtable_index_item_t* ii_a = (tosdb_memtable_index_item_t*)a;
    const tosdb_memtable_index_item_t* ii_b = (tosdb_memtable_index_item_t*)b;

    if(ii_a->record_id < ii_b->record_id) {
        return -1;
    } else if(ii_a->record_id > ii_b->record_id) {
        return 1;
    }

    return 0;
}

static int8_t tosdb_compaction_existing_set_sk_comparator(const void* a, const void* b) {
    if(!a || !b) {
        return 0;
    }

    const tosdb_memtable_secondary_index_item_t* ii_a = (tosdb_memtable_secondary_index_item_t*)a;
    const tosdb_memtable_secondary_index_item_t* ii_b = (tosdb_memtable_secondary_index_item_t*)b;

    if(ii_a->record_id < ii_b->record_id) {
        return -1;
    } else if(ii_a->record_id > ii_b->record_id) {
        return 1;
    }

    return 0;
}


static boolean_t tosdb_sstable_compact(const tosdb_table_t* tbl, const tosdb_index_t* idx, const tosdb_block_sstable_list_item_t* item, list_t* holes) {

    const compression_t* compression = tbl->db->tdb->compression;

    uint64_t idx_loc = 0;
    uint64_t idx_size = 0;

    for(uint64_t i = 0; i < item->index_count; i++) {
        if(idx->id == item->indexes[i].index_id) {
            idx_loc = item->indexes[i].index_location;
            idx_size = item->indexes[i].index_size;
            break;
        }
    }

    if(!idx_loc || !idx_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "index not found");

        return false;
    }

    tosdb_index_type_t idx_type = idx->type;

    tosdb_cache_t* tdb_cache = tbl->db->tdb->cache;

    uint64_t index_data_size = 0;
    uint64_t index_data_location = 0;

    tosdb_cached_bloomfilter_t* c_bf = NULL;

    tosdb_cache_key_t cache_key = {0};

    cache_key.type = TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER;
    cache_key.database_id = tbl->db->id;
    cache_key.table_id = tbl->id;
    cache_key.index_id = idx->id;
    cache_key.level = item->level;
    cache_key.sstable_id = item->sstable_id;

    if(tdb_cache) {
        c_bf = (tosdb_cached_bloomfilter_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }
    if(c_bf) {
        index_data_size = c_bf->index_data_size;
        index_data_location = c_bf->index_data_location;
    } else {
        tosdb_block_sstable_index_t* st_idx = (tosdb_block_sstable_index_t*)tosdb_block_read(tbl->db->tdb, idx_loc, idx_size);

        if(!st_idx) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read sstable index from backend");

            return false;
        }

        index_data_size = st_idx->index_data_size;
        index_data_location = st_idx->index_data_location;

        memory_free(st_idx);
    }

    void** st_idx_items = NULL;
    uint64_t valuelog_location = 0;
    uint64_t valuelog_size = 0;
    uint64_t record_count = 0;
    uint8_t* idx_data = NULL;
    uint8_t* org_idx_data = NULL;
    boolean_t idx_not_from_cache = false;

    tosdb_cached_index_data_t* c_id = NULL;


    cache_key.type = TOSDB_CACHE_ITEM_TYPE_INDEX_DATA;

    if(tdb_cache) {
        c_id = (tosdb_cached_index_data_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_id) {
        PRINTLOG(TOSDB, LOG_TRACE, "index data read from cache");

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            st_idx_items = (void**)c_id->index_items;
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            st_idx_items = (void**)c_id->secondary_index_items;
        } else {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot find index type");
            return false;
        }

        record_count = c_id->record_count;
        valuelog_location = c_id->valuelog_location;
        valuelog_size = c_id->valuelog_size;
    } else {
        idx_not_from_cache = true;
        valuelog_location = item->valuelog_location;
        valuelog_size = item->valuelog_size;

        tosdb_block_sstable_index_data_t* b_sid = (tosdb_block_sstable_index_data_t*)tosdb_block_read(tbl->db->tdb, index_data_location, index_data_size);

        if(!b_sid) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read index data");

            return false;

        }

        record_count = b_sid->record_count;

        buffer_t* buf_idx_in = buffer_encapsulate(b_sid->data, b_sid->index_data_size);
        buffer_t* buf_idx_out = buffer_new_with_capacity(NULL, b_sid->index_data_unpacked_size);

        int8_t zc_res = compression->unpack(buf_idx_in, buf_idx_out);

        uint64_t zc = buffer_get_length(buf_idx_out);

        uint64_t index_data_unpacked_size = b_sid->index_data_unpacked_size;

        memory_free(b_sid);

        buffer_destroy(buf_idx_in);

        if(zc_res != 0 || zc != index_data_unpacked_size) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot unpack idx");
            buffer_destroy(buf_idx_out);

            return false;
        }

        idx_data = buffer_get_all_bytes_and_destroy(buf_idx_out, NULL);
        org_idx_data = idx_data;

        uint64_t st_idx_items_len = 0;

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            st_idx_items_len = sizeof(tosdb_memtable_index_item_t*) * record_count;
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            st_idx_items_len = sizeof(tosdb_memtable_secondary_index_item_t*) * record_count;
        } else {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot find index type");
            memory_free(idx_data);

            return false;
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

    }

    buffer_t* buf_vl_out = NULL;
    tosdb_cached_valuelog_t* c_vl = NULL;
    boolean_t vl_not_from_cache = false;

    cache_key.type = TOSDB_CACHE_ITEM_TYPE_VALUELOG;

    if(tdb_cache) {
        c_vl = (tosdb_cached_valuelog_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_vl) {
        buf_vl_out = c_vl->values;
    } else {
        vl_not_from_cache = true;
        tosdb_block_valuelog_t* b_vl = (tosdb_block_valuelog_t*)tosdb_block_read(tbl->db->tdb, valuelog_location, valuelog_size);

        if(!b_vl) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read valuelog block");

            if(idx_not_from_cache || !tdb_cache) {
                memory_free(st_idx_items);
                memory_free(org_idx_data);
            }

            return false;
        }

        buffer_t* buf_vl_in = buffer_encapsulate(b_vl->data, b_vl->data_size);

        if(!buf_vl_in) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot encapsulate valuelog");

            if(idx_not_from_cache || !tdb_cache) {
                memory_free(st_idx_items);
                memory_free(org_idx_data);
            }

            return false;
        }

        buf_vl_out = buffer_new_with_capacity(NULL, b_vl->valuelog_unpacked_size);

        if(!buf_vl_out) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create valuelog buffer for decompress");

            if(idx_not_from_cache || !tdb_cache) {
                memory_free(st_idx_items);
                memory_free(org_idx_data);
            }

            return false;
        }

        uint64_t buf_vl_unpacked_size = b_vl->valuelog_unpacked_size;

        int8_t zc_res = compression->unpack(buf_vl_in, buf_vl_out);

        uint64_t zc = buffer_get_length(buf_vl_out);

        memory_free(b_vl);
        buffer_destroy(buf_vl_in);

        if(zc_res != 0 || zc != buf_vl_unpacked_size) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannotunpack valuelog");
            buffer_destroy(buf_vl_out);

            if(idx_not_from_cache || !tdb_cache) {
                memory_free(st_idx_items);
                memory_free(org_idx_data);
            }

            return false;
        }
    }

    set_t* hole_set = set_create(tosdb_record_key_comparator);

    boolean_t error = false;

    if(!hole_set) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create hole set");
        error = true;
        goto exit;
    }

    for(uint64_t i = 0; i < list_size(holes); i++) {
        const tosdb_record_t* rec = list_get_data_at_position(holes, i);

        if(!set_append(hole_set, (void*)rec)) {
            // TODO: may destroy rec?
        }
    }

    PRINTLOG(TOSDB, LOG_INFO, "hole count: %lli", set_size(hole_set));

    set_t* st_idx_set = NULL;

    if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
        st_idx_set = set_create(tosdb_compaction_existing_set_pk_comparator);
    } else if(idx_type == TOSDB_INDEX_SECONDARY) {
        st_idx_set = set_create(tosdb_compaction_existing_set_sk_comparator);
    }

    if(!st_idx_set) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index set");
        set_destroy(hole_set);
        error = true;
        goto exit;
    }

    for(uint64_t i = 0; i < record_count; i++) {
        tosdb_record_t t_rec = {0};
        tosdb_record_context_t t_ctx = {0};
        t_rec.context = &t_ctx;
        t_ctx.table = (tosdb_table_t*)tbl;

        if(idx_type == TOSDB_INDEX_PRIMARY || idx_type == TOSDB_INDEX_UNIQUE) {
            tosdb_memtable_index_item_t* ii = (tosdb_memtable_index_item_t*)st_idx_items[i];
            t_ctx.sstable_id = item->sstable_id;
            t_ctx.level = item->level;
            t_ctx.record_id = ii->record_id;
            t_ctx.offset = ii->offset;
            t_ctx.length = ii->length;
        } else if(idx_type == TOSDB_INDEX_SECONDARY) {
            tosdb_memtable_secondary_index_item_t* ii = (tosdb_memtable_secondary_index_item_t*)st_idx_items[i];
            t_ctx.sstable_id = item->sstable_id;
            t_ctx.level = item->level;
            t_ctx.record_id = ii->record_id;
            t_ctx.offset = ii->offset;
            t_ctx.length = ii->length;
        }


        if(!set_exists(hole_set, (void*)&t_rec)) {
            if(!set_append(st_idx_set, st_idx_items[i])) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot append index to set record id %llu%llu",
                         (uint64_t)(t_ctx.record_id >> 64), (uint64_t)t_ctx.record_id);

                error = true;
                break;
            }
        }
    }

    set_destroy(hole_set);

    if(error) {
        set_destroy(st_idx_set);
        goto exit;
    }


    PRINTLOG(TOSDB, LOG_INFO, "existing index count: %lli", set_size(st_idx_set));

    set_destroy(st_idx_set);

exit:

    if(idx_not_from_cache || !tdb_cache) {
        memory_free(st_idx_items);
        memory_free(org_idx_data);
    }

    if(vl_not_from_cache || !tdb_cache) {
        buffer_destroy(buf_vl_out);
    }

    return !error;
}

boolean_t tosdb_table_compact(const tosdb_table_t* tbl, tosdb_compaction_type_t type) {
    if(!tbl) {
        return false;
    }

    if(type == TOSDB_COMPACTION_TYPE_NONE) {
        return true;
    }

    if(tosdb_table_load_table((tosdb_table_t*)tbl) != tbl) {
        return false;
    }

    uint64_t compaction_index_id_hint = tbl->compaction_index_id_hint;

    if(compaction_index_id_hint == -1ULL) {
        compaction_index_id_hint = tbl->primary_index_id;
    }

    tosdb_index_t* idx = (tosdb_index_t*)hashmap_get(tbl->indexes, (void*)compaction_index_id_hint);

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find index %lli for compaction", compaction_index_id_hint);
        return false;
    }

    boolean_t error = false;

    set_t* keys = set_create(tosdb_record_key_comparator);
    list_t* old_keys = list_create_list();

    if(!tosdb_table_get_keys_internal(tbl, idx->id, keys, old_keys)) {
        error = true;
        set_destroy_with_callback(keys, tosdb_record_search_set_destroy_cb);

        return false;
    }

    PRINTLOG(TOSDB, LOG_INFO, "!!! ss %s live pk count: %lli", tbl->name, set_size(keys));
    set_destroy_with_callback(keys, tosdb_record_search_set_destroy_cb);

    PRINTLOG(TOSDB, LOG_INFO, "!!! ls %s dead pk count %lli", tbl->name, list_size(old_keys));

    iterator_t* iter = list_iterator_create(old_keys);

    hashmap_t* level_holes = hashmap_integer(128);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_t* rec = (tosdb_record_t*)iter->get_item(iter);
        tosdb_record_context_t* ctx = rec->context;

        hashmap_t* st_holes = (hashmap_t*)hashmap_get(level_holes, (void*)ctx->level);

        if(!st_holes) {
            st_holes = hashmap_integer(128);
            hashmap_put(level_holes, (void*)ctx->level, st_holes);
        }

        list_t* st_holes_list = (list_t*)hashmap_get(st_holes, (void*)ctx->sstable_id);

        if(!st_holes_list) {
            st_holes_list = list_create_sortedlist(tosdb_record_key_comparator);
            hashmap_put(st_holes, (void*)ctx->sstable_id, st_holes_list);
        }

        list_sortedlist_insert(st_holes_list, rec);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
    list_destroy(old_keys);

    iter = hashmap_iterator_create(level_holes);

    while(iter->end_of_iterator(iter) != 0) {
        hashmap_t* st_holes = (hashmap_t*)iter->get_item(iter);
        uint64_t level = (uint64_t)iter->get_extra_data(iter);

        iterator_t* st_iter = hashmap_iterator_create(st_holes);

        uint64_t total_hole_count = 0;

        while(st_iter->end_of_iterator(st_iter) != 0) {
            list_t* st_holes_list = (list_t*)st_iter->get_item(st_iter);
            uint64_t st_id = (uint64_t)st_iter->get_extra_data(st_iter);
            const tosdb_block_sstable_list_item_t* st_item = tosdb_table_find_sstable_list_item(tbl, level, st_id);

            if(!st_item) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot find sstable list item level: %lli id: %lli", level, st_id);
                error = true;

                list_destroy_with_type(st_holes_list, LIST_DESTROY_WITH_DATA, tosdb_record_list_destroyer_cb);

                st_iter = st_iter->next(st_iter);

                continue;
            }

            total_hole_count += list_size(st_holes_list);

            PRINTLOG(TOSDB, LOG_INFO, "level: %lli sstable id: %lli hole count: %lli/%lli",
                     level, st_id, list_size(st_holes_list), st_item->record_count);

            error |= !tosdb_sstable_compact(tbl, idx, st_item, st_holes_list);

            list_destroy_with_type(st_holes_list, LIST_DESTROY_WITH_DATA, tosdb_record_list_destroyer_cb);

            st_iter = st_iter->next(st_iter);
        }

        PRINTLOG(TOSDB, LOG_INFO, "level: %lli total hole count: %lli", level, total_hole_count);

        st_iter->destroy(st_iter);

        hashmap_destroy(st_holes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    hashmap_destroy(level_holes);

    UNUSED(type);
/*
    for(uint64_t i = 1; i <= tbl->sstable_max_level; i++) {
        if(type == TOSDB_COMPACTION_TYPE_MINOR) {
            error = !tosdb_sstable_level_minor_compact(tbl, i);
        } else {
            error = !tosdb_sstable_level_major_compact(tbl, i);
        }
    }
 */
    return !error;
}

boolean_t tosdb_sstable_level_minor_compact(const tosdb_table_t* tbl, uint64_t level) {
    UNUSED(tbl);
    UNUSED(level);
    NOTIMPLEMENTEDLOG(TOSDB);
    return false;
}

boolean_t tosdb_sstable_level_major_compact(const tosdb_table_t* tbl, uint64_t level) {
    UNUSED(tbl);
    UNUSED(level);
    NOTIMPLEMENTEDLOG(TOSDB);
    return false;
}
