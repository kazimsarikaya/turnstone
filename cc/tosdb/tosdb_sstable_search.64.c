/**
 * @file tosdb_sstable_search.64.c
 * @brief tosdb sstable interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_cache.h>
#include <logging.h>
#include <compression.h>
#include <binarysearch.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_sstable_search_on_list(tosdb_record_t * record, set_t* results, list_t* st_list, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id);
boolean_t tosdb_sstable_search_on_index(tosdb_record_t * record, set_t* results, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id);
int8_t    tosdb_sstable_secondary_index_comparator(const void* i1, const void* i2);

int8_t tosdb_sstable_secondary_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_secondary_index_item_t* ti1 = (tosdb_memtable_secondary_index_item_t*)*((void**)i1);
    const tosdb_memtable_secondary_index_item_t* ti2 = (tosdb_memtable_secondary_index_item_t*)*((void**)i2);

    if(!ti1 && ti2) {
        return -1;
    }

    if(ti1 && !ti2) {
        return 1;
    }

    if(ti1->secondary_key_hash < ti2->secondary_key_hash) {
        return -1;
    }

    if(ti1->secondary_key_hash > ti2->secondary_key_hash) {
        return 1;
    }

    if(!ti1->secondary_key_length && !ti1->secondary_key_length) {
        return 0;
    }

    uint64_t min = MIN(ti1->secondary_key_length, ti2->secondary_key_length);

    int8_t res = memory_memcompare(ti1->data, ti2->data, min);

    if(res != 0) {
        return res;
    }

    if(ti1->secondary_key_length < ti2->secondary_key_length) {
        return -1;
    }

    if(ti1->secondary_key_length > ti2->secondary_key_length) {
        return 1;
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_sstable_search_on_index(tosdb_record_t * record, set_t* results, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id){
    tosdb_record_context_t* ctx = record->context;

    const compression_t* compression = ctx->table->db->tdb->compression;

    uint64_t idx_loc = 0;
    uint64_t idx_size = 0;

    for(uint64_t i = 0; i < sli->index_count; i++) {
        if(index_id == sli->indexes[i].index_id) {
            idx_loc = sli->indexes[i].index_location;
            idx_size = sli->indexes[i].index_size;

        }
    }

    if(!idx_loc || !idx_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "index not found");

        return false;
    }

    tosdb_cache_t* tdb_cache = ctx->table->db->tdb->cache;

    tosdb_memtable_secondary_index_item_t* first = NULL;
    tosdb_memtable_secondary_index_item_t* last = NULL;
    bloomfilter_t* bf = NULL;
    uint64_t index_data_size = 0;
    uint64_t index_data_location = 0;
    uint64_t record_count = 0;

    tosdb_cached_bloomfilter_t* c_bf = NULL;

    tosdb_cache_key_t cache_key = {0};

    cache_key.type = TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER;
    cache_key.database_id = ctx->table->db->id;
    cache_key.table_id = ctx->table->id;
    cache_key.index_id = index_id;
    cache_key.level = sli->level;
    cache_key.sstable_id = sli->sstable_id;

    if(tdb_cache) {
        c_bf = (tosdb_cached_bloomfilter_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_bf) {
        first = c_bf->secondary_first_key;
        last = c_bf->secondary_last_key;
        bf = c_bf->bloomfilter;
        index_data_size = c_bf->index_data_size;
        index_data_location = c_bf->index_data_location;
    } else {
        tosdb_block_sstable_index_t* st_idx = (tosdb_block_sstable_index_t*)tosdb_block_read(ctx->table->db->tdb, idx_loc, idx_size);

        if(!st_idx) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read sstable index from backend");

            return false;
        }

        record_count = st_idx->record_count;

        tosdb_memtable_secondary_index_item_t* t_first = (tosdb_memtable_secondary_index_item_t*)st_idx->data;

        uint64_t first_key_length = t_first->secondary_key_length + t_first->primary_key_length + sizeof(tosdb_memtable_secondary_index_item_t);
        first = memory_malloc(first_key_length);

        if(!first) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate first item");
            memory_free(st_idx);

            return false;
        }

        memory_memcopy(t_first, first, first_key_length);

        tosdb_memtable_secondary_index_item_t* t_last = (tosdb_memtable_secondary_index_item_t*)(st_idx->data + sizeof(tosdb_memtable_index_item_t) + first->secondary_key_length + first->primary_key_length);

        uint64_t last_key_length = t_last->secondary_key_length + t_last->primary_key_length + sizeof(tosdb_memtable_index_item_t);
        last = memory_malloc(last_key_length);

        if(!last) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate last item");
            memory_free(first);
            memory_free(st_idx);

            return false;
        }

        memory_memcopy(t_last, last, last_key_length);

        buffer_t* buf_bf_in = buffer_encapsulate(st_idx->data + st_idx->minmax_key_size, st_idx->bloomfilter_size);
        buffer_t* buf_bf_out = buffer_new_with_capacity(NULL, st_idx->bloomfilter_unpacked_size);

        int8_t zc_res = compression->unpack(buf_bf_in, buf_bf_out);

        uint64_t zc = buffer_get_length(buf_bf_out);

        buffer_destroy(buf_bf_in);

        if(zc_res != 0 || zc != st_idx->bloomfilter_unpacked_size) {
            PRINTLOG(TOSDB, LOG_ERROR, "table %s, stli id %lli, index id %lli", ctx->table->name, sli->sstable_id, index_id);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot unpack bf zc_res: %i, zc: 0x%llx, unpacked_size: 0x%llx, packed size: 0x%llx", zc_res, zc, st_idx->bloomfilter_unpacked_size, st_idx->bloomfilter_size);

            memory_free(st_idx);
            buffer_destroy(buf_bf_out);
            memory_free(first);
            memory_free(last);

            return false;
        }

        uint64_t bf_data_len = 0;
        uint8_t* bf_data = buffer_get_all_bytes_and_destroy(buf_bf_out, &bf_data_len);

        data_t bf_tmp_d = {0};
        bf_tmp_d.type = DATA_TYPE_INT8_ARRAY;
        bf_tmp_d.length = bf_data_len;
        bf_tmp_d.value = bf_data;

        bf = bloomfilter_deserialize(&bf_tmp_d);

        memory_free(bf_data);

        if(!bf) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot deserialize bloom filter");
            memory_free(st_idx);

            return false;
        }

        if(tdb_cache) {
            c_bf = memory_malloc(sizeof(tosdb_cached_bloomfilter_t));

            if(!c_bf) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate cached bloom filter");
                memory_free(first);
                memory_free(last);
                memory_free(st_idx);
                bloomfilter_destroy(bf);

                return false;
            }

            memory_memcopy(&cache_key, c_bf, sizeof(tosdb_cache_key_t));
            c_bf->index_data_location = st_idx->index_data_location;
            c_bf->index_data_size = st_idx->index_data_size;
            c_bf->bloomfilter = bf;
            c_bf->secondary_first_key = first;
            c_bf->secondary_last_key = last;

            c_bf->cache_key.data_size = sizeof(tosdb_cached_bloomfilter_t) + st_idx->bloomfilter_unpacked_size + first_key_length + last_key_length + 64; // near size

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_bf);
        }

        index_data_size = st_idx->index_data_size;
        index_data_location = st_idx->index_data_location;

        memory_free(st_idx);
    }

    PRINTLOG(TOSDB, LOG_TRACE, "sstable 0x%llx level 0x%llx first: %llx item %llx last: %llx",
             sli->sstable_id, sli->level, first->secondary_key_hash, item->secondary_key_hash, last->secondary_key_hash);

    int8_t first_limit = tosdb_sstable_secondary_index_comparator(&first, &item);
    int8_t last_limit = tosdb_sstable_secondary_index_comparator(&last, &item);

    if(first_limit == 1 || last_limit == -1) {

        PRINTLOG(TOSDB, LOG_TRACE, "not found inside sstable 0x%llx level 0x%llx first_limit: %d last_limit: %d",
                 sli->sstable_id, sli->level, first_limit, last_limit);

        if(!tdb_cache) {
            bloomfilter_destroy(bf);
            memory_free(first);
            memory_free(last);
        }

        return true;
    }

    if(!tdb_cache) {
        memory_free(first);
        memory_free(last);
    }

    uint8_t* u8_key = item->data;
    uint64_t u8_key_length = item->secondary_key_length;

    if(!u8_key_length) {
        u8_key_length = sizeof(uint64_t);
        u8_key = (uint8_t*)&item->secondary_key_hash;
    }

    data_t item_tmp_data = {0};
    item_tmp_data.type = DATA_TYPE_INT8_ARRAY;
    item_tmp_data.length = u8_key_length;
    item_tmp_data.value = u8_key;

    if(!bloomfilter_check(bf, &item_tmp_data)) {
        if(!tdb_cache) {
            bloomfilter_destroy(bf);
        }

        PRINTLOG(TOSDB, LOG_TRACE, "sstable 0x%llx level 0x%llx not found at bloom filter", sli->sstable_id, sli->level);

        return true;
    }

    if(!tdb_cache) {
        bloomfilter_destroy(bf);
    }

    tosdb_memtable_secondary_index_item_t** st_idx_items = NULL;

    uint8_t* idx_data = NULL;
    uint8_t* org_idx_data = NULL;

    tosdb_cached_secondary_index_data_t* c_id = NULL;


    cache_key.type = TOSDB_CACHE_ITEM_TYPE_SECONDARY_INDEX_DATA;

    if(tdb_cache) {
        c_id = (tosdb_cached_secondary_index_data_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_id) {
        st_idx_items = c_id->index_items;
        record_count = c_id->record_count;

    } else {
        tosdb_block_sstable_index_data_t* b_sid = (tosdb_block_sstable_index_data_t*)tosdb_block_read(ctx->table->db->tdb, index_data_location, index_data_size);

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
            PRINTLOG(TOSDB, LOG_ERROR, "table %s, stli id %lli, index id %lli", ctx->table->name, sli->sstable_id, index_id);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot unpack idx data, zc_res: %d, zc: %llu, index_data_unpacked_size: %llu", zc_res, zc, index_data_unpacked_size);
            buffer_destroy(buf_idx_out);

            return false;
        }

        idx_data = buffer_get_all_bytes_and_destroy(buf_idx_out, NULL);
        org_idx_data = idx_data;

        st_idx_items = memory_malloc(sizeof(tosdb_memtable_secondary_index_item_t*) * record_count);

        if(!st_idx_items) {
            memory_free(idx_data);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");

            return false;
        }

        for(uint64_t i = 0; i < record_count; i++) {
            st_idx_items[i] = (tosdb_memtable_secondary_index_item_t*)idx_data;

            if(!st_idx_items[i]) {
                memory_free(st_idx_items);
                memory_free(idx_data);

                PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item 0x%llx", i);

                return false;
            }

            idx_data += sizeof(tosdb_memtable_secondary_index_item_t) + st_idx_items[i]->secondary_key_length + st_idx_items[i]->primary_key_length;
        }

        if(tdb_cache) {
            c_id = memory_malloc(sizeof(tosdb_cached_secondary_index_data_t));

            if(!c_id) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate cached secondary index data");
                memory_free(st_idx_items);
                memory_free(idx_data);

                return false;
            }

            memory_memcopy(&cache_key, c_id, sizeof(tosdb_cache_key_t));
            c_id->index_items = st_idx_items;
            c_id->record_count = record_count;
            c_id->cache_key.data_size = sizeof(tosdb_cached_secondary_index_data_t) + index_data_unpacked_size + sizeof(tosdb_memtable_secondary_index_item_t*) * record_count;

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_id);
        }
    }

    PRINTLOG(TOSDB, LOG_TRACE, "index data read, record count: 0x%llx", record_count);

    tosdb_memtable_secondary_index_item_t** found_item = (tosdb_memtable_secondary_index_item_t**)binarysearch(st_idx_items,
                                                                                                               record_count,
                                                                                                               sizeof(tosdb_memtable_secondary_index_item_t*),
                                                                                                               &item,
                                                                                                               tosdb_sstable_secondary_index_comparator);

    tosdb_memtable_secondary_index_item_t** org_found_item = found_item;

    boolean_t error = false;

    while(found_item && found_item < st_idx_items + record_count) {
        if(tosdb_sstable_secondary_index_comparator(&item, found_item)) {
            break;
        }

        tosdb_memtable_secondary_index_item_t* s_idx_item = *found_item;

        uint64_t idx_item_len = sizeof(tosdb_memtable_index_item_t) + s_idx_item->primary_key_length;

        tosdb_memtable_index_item_t* res = memory_malloc(idx_item_len);

        if(!res) {
            error = true;

            break;
        }

        res->record_id = s_idx_item->record_id;
        res->is_deleted = s_idx_item->is_primary_key_deleted;
        res->key_hash = s_idx_item->primary_key_hash;
        res->key_length = s_idx_item->primary_key_length;
        memory_memcopy(s_idx_item->data + s_idx_item->secondary_key_length, res->key, res->key_length);

        if(res->key_hash == 7083) {
            PRINTLOG(TOSDB, LOG_TRACE, "found item: %s deleted? %i", res->key, res->is_deleted);
        }

        if(!set_append(results, res)) {
            memory_free(res);
        }

        found_item++;
    }

    if(!error) {
        found_item = org_found_item;

        while(found_item && found_item >= st_idx_items) {
            if(tosdb_sstable_secondary_index_comparator(&item, found_item)) {
                break;
            }

            tosdb_memtable_secondary_index_item_t* s_idx_item = *found_item;

            uint64_t idx_item_len = sizeof(tosdb_memtable_index_item_t) + s_idx_item->primary_key_length;

            tosdb_memtable_index_item_t* res = memory_malloc(idx_item_len);

            if(!res) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index item");
                error = true;

                break;
            }

            res->record_id = s_idx_item->record_id;
            res->is_deleted = s_idx_item->is_primary_key_deleted;
            res->key_hash = s_idx_item->primary_key_hash;
            res->key_length = s_idx_item->primary_key_length;
            memory_memcopy(s_idx_item->data + s_idx_item->secondary_key_length, res->key, res->key_length);

            if(res->key_hash == 7083) {
                PRINTLOG(TOSDB, LOG_TRACE, "found item: %s deleted? %i", res->key, res->is_deleted);
            }



            if(!set_append(results, res)) {
                memory_free(res);
            }

            found_item--;
        }

    }

    if(!tdb_cache) {
        memory_free(st_idx_items);
        memory_free(org_idx_data);
    }

    return !error;
}
#pragma GCC diagnostic pop

boolean_t tosdb_sstable_search_on_list(tosdb_record_t * record, set_t* results, list_t* st_list, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id) {
    boolean_t error = false;

    iterator_t* iter = list_iterator_create(st_list);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstables list items iterator");

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_block_sstable_list_item_t* sli = (tosdb_block_sstable_list_item_t*) iter->get_item(iter);

        if(index_id <= sli->index_count) {
            error |= !tosdb_sstable_search_on_index(record, results, sli, item, index_id);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return !error;
}

boolean_t tosdb_sstable_search(tosdb_record_t* record, set_t* results) {
    if(!record || !record->context) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    if(hashmap_size(ctx->keys) != 1) {
        PRINTLOG(TOSDB, LOG_ERROR, "record search supports only one key");

        return false;
    }

    iterator_t* iter = hashmap_iterator_create(ctx->keys);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get key");

        return false;
    }

    const tosdb_record_key_t* r_key = iter->get_item(iter);

    iter->destroy(iter);

    tosdb_memtable_secondary_index_item_t* item = memory_malloc(sizeof(tosdb_memtable_secondary_index_item_t) + r_key->key_length);

    if(!item) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index item");

        return false;
    }

    item->secondary_key_hash = r_key->key_hash;
    item->secondary_key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->data, item->secondary_key_length);

    if(ctx->table->sstable_list_items && !tosdb_sstable_search_on_list(record, results, ctx->table->sstable_list_items, item, r_key->index_id)) {
        memory_free(item);

        return false;
    }


    if(ctx->table->sstable_levels) {
        for(uint64_t i = 1; i <= ctx->table->sstable_max_level; i++) {
            list_t* st_lvl_l = (list_t*)hashmap_get(ctx->table->sstable_levels, (void*)i);

            if(st_lvl_l) {
                if(!tosdb_sstable_search_on_list(record, results, st_lvl_l, item, r_key->index_id)) {
                    memory_free(item);

                    return false;
                }
            }
        }
    }

    memory_free(item);

    return true;
}

