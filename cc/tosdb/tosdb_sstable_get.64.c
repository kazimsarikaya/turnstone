/**
 * @file tosdb_sstable_get.64.c
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

boolean_t tosdb_sstable_get_on_list(tosdb_record_t * record, list_t* st_list, tosdb_memtable_index_item_t* item, uint64_t index_id);
boolean_t tosdb_sstable_get_on_index(tosdb_record_t * record, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_index_item_t* item, uint64_t index_id);

static int8_t tosdb_sstable_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_index_item_t* ti1 = (tosdb_memtable_index_item_t*)*((void**)i1);
    const tosdb_memtable_index_item_t* ti2 = (tosdb_memtable_index_item_t*)*((void**)i2);

    if(!ti1 && ti2) {
        return -1;
    }

    if(ti1 && !ti2) {
        return 1;
    }

    if(ti1->key_hash < ti2->key_hash) {
        return -1;
    }

    if(ti1->key_hash > ti2->key_hash) {
        return 1;
    }

    if(!ti1->key_length && !ti1->key_length) {
        return 0;
    }

    uint64_t min = MIN(ti1->key_length, ti2->key_length);

    int8_t res = memory_memcompare(ti1->key, ti2->key, min);

    if(res != 0) {
        return res;
    }

    if(ti1->key_length < ti2->key_length) {
        return -1;
    }

    if(ti1->key_length > ti2->key_length) {
        return 1;
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_sstable_get_on_index(tosdb_record_t * record, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_index_item_t* item, uint64_t index_id){
    tosdb_record_context_t* ctx = record->context;

    const compression_t* compression = ctx->table->db->tdb->compression;

    uint64_t idx_loc = 0;
    uint64_t idx_size = 0;

    for(uint64_t i = 0; i < sli->index_count; i++) {
        if(index_id == sli->indexes[i].index_id) {
            idx_loc = sli->indexes[i].index_location;
            idx_size = sli->indexes[i].index_size;
            break;
        }
    }

    if(!idx_loc || !idx_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "index not found");

        return false;
    }

    tosdb_cache_t* tdb_cache = ctx->table->db->tdb->cache;

    tosdb_memtable_index_item_t* first = NULL;
    tosdb_memtable_index_item_t* last = NULL;
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
        first = c_bf->first_key;
        last = c_bf->last_key;
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

        uint8_t* st_idx_data = &st_idx->data[0];

        tosdb_memtable_index_item_t* t_first = (tosdb_memtable_index_item_t*)st_idx_data;

        uint64_t first_key_length = t_first->key_length + sizeof(tosdb_memtable_index_item_t);
        first = memory_malloc(first_key_length);

        if(!first) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate first item");
            memory_free(st_idx);

            return false;
        }

        PRINTLOG(TOSDB, LOG_TRACE, "first key length 0x%llx", first_key_length);

        memory_memcopy(t_first, first, first_key_length);

        st_idx_data += first_key_length;

        tosdb_memtable_index_item_t* t_last = (tosdb_memtable_index_item_t*)st_idx_data;

        uint64_t last_key_length = t_last->key_length + sizeof(tosdb_memtable_index_item_t);
        last = memory_malloc(last_key_length);

        if(!last) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate last item 0x%llx 0x%llx", last_key_length, t_last->key_length);;
            memory_free(first);
            memory_free(st_idx);

            return false;
        }

        memory_memcopy(t_last, last, last_key_length);

        st_idx_data += last_key_length;

        buffer_t* buf_bf_in = buffer_encapsulate(st_idx_data, st_idx->bloomfilter_size);
        buffer_t* buf_bf_out = buffer_new_with_capacity(NULL, st_idx->bloomfilter_unpacked_size);

        int8_t zc_res = compression->unpack(buf_bf_in, buf_bf_out);

        uint64_t zc = buffer_get_length(buf_bf_out);

        buffer_destroy(buf_bf_in);

        if(zc_res != 0 || zc != st_idx->bloomfilter_unpacked_size) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot unpack bf");
            memory_free(first);
            memory_free(last);
            memory_free(st_idx);
            buffer_destroy(buf_bf_out);

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
            c_bf->first_key = first;
            c_bf->last_key = last;

            c_bf->cache_key.data_size = sizeof(tosdb_cached_bloomfilter_t) + st_idx->bloomfilter_unpacked_size + first_key_length + last_key_length + 64; // near size

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_bf);
        }

        index_data_size = st_idx->index_data_size;
        index_data_location = st_idx->index_data_location;

        memory_free(st_idx);
    }


    PRINTLOG(TOSDB, LOG_TRACE, "sstable 0x%llx level 0x%llx first: %llx item %llx last: %llx",
             sli->sstable_id, sli->level, first->key_hash, item->key_hash, last->key_hash);

    int8_t first_limit = tosdb_sstable_index_comparator(&first, &item);
    int8_t last_limit = tosdb_sstable_index_comparator(&last, &item);

    if(first_limit == 1 || last_limit == -1) {
        if(!tdb_cache) {
            bloomfilter_destroy(bf);
            memory_free(first);
            memory_free(last);
        }

        PRINTLOG(TOSDB, LOG_TRACE, "not found inside sstable 0x%llx level 0x%llx first_limit: %d last_limit: %d", sli->sstable_id, sli->level, first_limit, last_limit);

        return false;
    }

    if(!tdb_cache) {
        memory_free(first);
        memory_free(last);
    }

    uint8_t* u8_key = item->key;
    uint64_t u8_key_length = item->key_length;

    if(!u8_key_length) {
        u8_key_length = sizeof(uint64_t);
        u8_key = (uint8_t*)&item->key_hash;
    }

    data_t item_tmp_data = {0};
    item_tmp_data.type = DATA_TYPE_INT8_ARRAY;
    item_tmp_data.length = u8_key_length;
    item_tmp_data.value = u8_key;

    if(!bloomfilter_check(bf, &item_tmp_data)) {
        if(!tdb_cache) {
            bloomfilter_destroy(bf);
        }

        PRINTLOG(TOSDB, LOG_TRACE, "not found inside sstable 0x%llx level 0x%llx bloomfilter", sli->sstable_id, sli->level);

        return false;
    }

    if(!tdb_cache) {
        bloomfilter_destroy(bf);
    }

    tosdb_memtable_index_item_t** st_idx_items = NULL;
    uint64_t valuelog_location = 0;
    uint64_t valuelog_size = 0;
    uint8_t* idx_data = NULL;
    uint8_t* org_idx_data = NULL;

    tosdb_cached_index_data_t* c_id = NULL;


    cache_key.type = TOSDB_CACHE_ITEM_TYPE_INDEX_DATA;

    if(tdb_cache) {
        c_id = (tosdb_cached_index_data_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_id) {
        PRINTLOG(TOSDB, LOG_TRACE, "index data read from cache");
        st_idx_items = c_id->index_items;
        record_count = c_id->record_count;
        valuelog_location = c_id->valuelog_location;
        valuelog_size = c_id->valuelog_size;
    } else {
        PRINTLOG(TOSDB, LOG_TRACE, "index data read from backend");
        valuelog_location = sli->valuelog_location;
        valuelog_size = sli->valuelog_size;

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
            PRINTLOG(TOSDB, LOG_ERROR, "cannot unpack idx");
            buffer_destroy(buf_idx_out);

            return false;
        }

        idx_data = buffer_get_all_bytes_and_destroy(buf_idx_out, NULL);
        org_idx_data = idx_data;

        st_idx_items = memory_malloc(sizeof(tosdb_memtable_index_item_t*) * record_count);

        if(!st_idx_items) {
            memory_free(idx_data);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");

            return false;
        }

        for(uint64_t i = 0; i < record_count; i++) {
            st_idx_items[i] = (tosdb_memtable_index_item_t*)idx_data;

            if(!st_idx_items[i]) {
                memory_free(st_idx_items);
                memory_free(idx_data);
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item 0x%llx", i);

                return false;
            }

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
            c_id->valuelog_location = valuelog_location;
            c_id->valuelog_size = valuelog_size;
            c_id->cache_key.data_size = sizeof(tosdb_cached_index_data_t) + index_data_unpacked_size + sizeof(tosdb_memtable_index_item_t*) * record_count;

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_id);
        }
    }

    PRINTLOG(TOSDB, LOG_TRACE, "index data read, record count: 0x%llx 0x%llx", record_count, record_count);

    tosdb_memtable_index_item_t** t_found_item = (tosdb_memtable_index_item_t**)binarysearch(st_idx_items,
                                                                                             record_count,
                                                                                             sizeof(tosdb_memtable_index_item_t*),
                                                                                             &item,
                                                                                             tosdb_sstable_index_comparator);


    if(!t_found_item) {
        if(!tdb_cache) {
            memory_free(st_idx_items);
            memory_free(org_idx_data);
        }

        return false;
    }

    tosdb_memtable_index_item_t* found_item = *t_found_item;

    if(!found_item) {
        if(!tdb_cache) {
            memory_free(st_idx_items);
            memory_free(org_idx_data);
        }

        return false;
    }

    ctx->record_id = found_item->record_id;

    if(found_item->is_deleted) {
        if(!tdb_cache) {
            memory_free(st_idx_items);
            memory_free(org_idx_data);
        }

        ctx->is_deleted = true;
        ctx->level = sli->level;
        ctx->sstable_id = sli->sstable_id;
        ctx->record_id = found_item->record_id;
        ctx->offset = -1ULL;
        ctx->length = 0;

        return true;
    }


    uint64_t offset = found_item->offset;
    uint64_t length = found_item->length;

    if(!tdb_cache) {
        memory_free(st_idx_items);
        memory_free(org_idx_data);
    }

    buffer_t* buf_vl_out = NULL;
    tosdb_cached_valuelog_t* c_vl = NULL;

    cache_key.type = TOSDB_CACHE_ITEM_TYPE_VALUELOG;

    if(tdb_cache) {
        c_vl = (tosdb_cached_valuelog_t*)tosdb_cache_get(tdb_cache, &cache_key);
    }

    if(c_vl) {
        buf_vl_out = c_vl->values;
    } else {
        tosdb_block_valuelog_t* b_vl = (tosdb_block_valuelog_t*)tosdb_block_read(ctx->table->db->tdb, valuelog_location, valuelog_size);

        if(!b_vl) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read valuelog block");

            return false;
        }

        buffer_t* buf_vl_in = buffer_encapsulate(b_vl->data, b_vl->data_size);

        if(!buf_vl_in) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot encapsulate valuelog");

            return false;
        }

        buf_vl_out = buffer_new_with_capacity(NULL, b_vl->valuelog_unpacked_size);

        if(!buf_vl_out) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create valuelog buffer for decompress");

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

            return false;
        }

        if(tdb_cache) {
            c_vl = memory_malloc(sizeof(tosdb_cached_valuelog_t));

            if(!c_vl) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate cached valuelog");
                buffer_destroy(buf_vl_out);

                return false;
            }

            memory_memcopy(&cache_key, c_vl, sizeof(tosdb_cache_key_t));
            c_vl->values = buf_vl_out;
            c_vl->cache_key.data_size = sizeof(tosdb_cached_valuelog_t) + buffer_get_length(c_vl->values);

            tosdb_cache_put(tdb_cache, (tosdb_cache_key_t*)c_vl);
        }
    }

    buffer_seek(buf_vl_out, offset, BUFFER_SEEK_DIRECTION_START);
    uint8_t* value_data = buffer_get_bytes(buf_vl_out, length);

    if(!tdb_cache) {
        buffer_destroy(buf_vl_out);
    }

    if(!value_data) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read value data from valuelog");

        return false;
    }

    data_t s_d = {0};
    s_d.length = length;
    s_d.type = DATA_TYPE_INT8_ARRAY;
    s_d.value = value_data;

    data_t* r_d = data_bson_deserialize(&s_d);

    memory_free(value_data);

    if(!r_d) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot deserialize data");

        return false;
    }

    tosdb_index_t* idx = (tosdb_index_t*)hashmap_get(ctx->table->indexes, (void*)index_id);

    data_t* tmp = r_d->value;

    for(uint64_t i = 0; i < r_d->length; i++) {
        uint64_t tmp_col_id = (uint64_t)tmp[i].name->value;

        if(tmp_col_id == idx->column_id) {
            continue;
        }

        if(!tosdb_record_set_data_with_colid(record, tmp_col_id, tmp[i].type, tmp[i].length, tmp[i].value)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot populate record");
        }
    }

    ctx->level = sli->level;
    ctx->sstable_id = sli->sstable_id;
    ctx->offset = found_item->offset;
    ctx->length = found_item->length;
    ctx->record_id = found_item->record_id;

    data_free(r_d);

    return true;
}
#pragma GCC diagnostic pop

boolean_t tosdb_sstable_get_on_list(tosdb_record_t * record, list_t* st_list, tosdb_memtable_index_item_t* item, uint64_t index_id) {
    tosdb_record_context_t* ctx = record->context;

    uint64_t cached_level = ctx->level;
    uint64_t cached_sstable_id = ctx->sstable_id;

    boolean_t found = false;

    iterator_t* iter = list_iterator_create(st_list);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstables list items iterator");

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_block_sstable_list_item_t* sli = (tosdb_block_sstable_list_item_t*) iter->get_item(iter);

        if(index_id <= sli->index_count) {
            PRINTLOG(TOSDB, LOG_TRACE, "found sstable 0x%llx list item with index id 0x%llx, searching", sli->sstable_id, index_id);

            if(cached_level != -1ULL && cached_sstable_id != -1ULL) {
                // we have cached sstable id and level check sstable id and level
                // if it is the same, we search on it. otherwise we iterate next one

                if(cached_level == sli->level && cached_sstable_id == sli->sstable_id) {

                    if(tosdb_sstable_get_on_index(record, sli, item, index_id)) {
                        found = true;
                    } else {
                        PRINTLOG(TOSDB, LOG_ERROR, "record not found in known sstable 0x%llx level 0x%llx", sli->sstable_id, sli->level);
                    }

                    break; // regardless of found, we break the loop because record cannot be other than this sstable
                }
            } else { // no cached sstable id and level so search on all sstables
                if(tosdb_sstable_get_on_index(record, sli, item, index_id)) {
                    found = true;

                    break;
                }
            }

        } else {
            PRINTLOG(TOSDB, LOG_TRACE, "skipping sstable 0x%llx list item with index id 0x%llx max index count 0x%llx", sli->sstable_id, index_id, sli->index_count);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return found;
}

boolean_t tosdb_sstable_get(tosdb_record_t* record) {
    if(!record || !record->context) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

    uint64_t cached_level = ctx->level;
    uint64_t cached_sstable_id = ctx->sstable_id;

    PRINTLOG(TOSDB, LOG_TRACE, "sstable get level 0x%llx sstable id 0x%llx", cached_level, cached_sstable_id);

    if(hashmap_size(ctx->keys) != 1) {
        PRINTLOG(TOSDB, LOG_ERROR, "record get supports only one key");

        return false;
    }

    iterator_t* iter = hashmap_iterator_create(ctx->keys);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot get key");

        return false;
    }

    const tosdb_record_key_t* r_key = iter->get_item(iter);

    iter->destroy(iter);

    tosdb_memtable_index_item_t* item = memory_malloc(sizeof(tosdb_memtable_index_item_t) + r_key->key_length);

    if(!item) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index item");

        return false;
    }

    item->key_hash = r_key->key_hash;
    item->key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->key, item->key_length);

    if(ctx->table->sstable_list_items && tosdb_sstable_get_on_list(record, ctx->table->sstable_list_items, item, r_key->index_id)) {
        memory_free(item);

        return true;
    }


    if(ctx->table->sstable_levels) {
        if(cached_level != -1ULL && cached_sstable_id != -1ULL) {
            PRINTLOG(TOSDB, LOG_TRACE, "searching on known sstable level 0x%llx", cached_level);

            list_t* st_lvl_l = (list_t*)hashmap_get(ctx->table->sstable_levels, (void*)cached_level);

            if(st_lvl_l) {
                if(tosdb_sstable_get_on_list(record, st_lvl_l, item, r_key->index_id)) {
                    memory_free(item);

                    return true;
                }

                PRINTLOG(TOSDB, LOG_ERROR, "record not found in known sstable level 0x%llx", cached_level);
                memory_free(item);

                return false;
            }

            PRINTLOG(TOSDB, LOG_ERROR, "sstable level 0x%llx not found", cached_level);
            memory_free(item);

            return false;
        }

        PRINTLOG(TOSDB, LOG_TRACE, "searching on sstable levels");

        for(uint64_t i = 1; i <= ctx->table->sstable_max_level; i++) {
            list_t* st_lvl_l = (list_t*)hashmap_get(ctx->table->sstable_levels, (void*)i);

            if(st_lvl_l) {
                PRINTLOG(TOSDB, LOG_TRACE, "searching on sstable level 0x%llx", i);

                if(tosdb_sstable_get_on_list(record, st_lvl_l, item, r_key->index_id)) {
                    memory_free(item);

                    return true;
                }
            }
        }
    }

    memory_free(item);

    PRINTLOG(TOSDB, LOG_TRACE, "record not found");

    return false;
}

