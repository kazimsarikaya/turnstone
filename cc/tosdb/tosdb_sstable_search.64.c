/**
 * @file tosdb_sstable.64.c
 * @brief tosdb sstable interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <zpack.h>
#include <binarysearch.h>

boolean_t tosdb_sstable_search_on_list(tosdb_record_t * record, set_t* results, linkedlist_t st_list, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id);
boolean_t tosdb_sstable_search_on_index(tosdb_record_t * record, set_t* results, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id);
int8_t    tosdb_sstable_secondary_index_comparator(const void* i1, const void* i2);

int8_t tosdb_sstable_secondary_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_secondary_index_item_t* ti1 = (tosdb_memtable_secondary_index_item_t*)*((void**)i1);
    const tosdb_memtable_secondary_index_item_t* ti2 = (tosdb_memtable_secondary_index_item_t*)*((void**)i2);

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

boolean_t tosdb_sstable_search_on_index(tosdb_record_t * record, set_t* results, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id){
    tosdb_record_context_t* ctx = record->context;

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

    tosdb_block_sstable_index_t* st_idx = (tosdb_block_sstable_index_t*)tosdb_block_read(ctx->table->db->tdb, idx_loc, idx_size);

    if(!st_idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read sstable index from backend");

        return false;
    }

    tosdb_memtable_secondary_index_item_t* first = (tosdb_memtable_secondary_index_item_t*)st_idx->data;
    tosdb_memtable_secondary_index_item_t* last = (tosdb_memtable_secondary_index_item_t*)(st_idx->data +
                                                                                           sizeof(tosdb_memtable_secondary_index_item_t) +
                                                                                           first->secondary_key_length +
                                                                                           first->primary_key_length);

    int8_t first_limit = tosdb_sstable_secondary_index_comparator(&first, &item);
    int8_t last_limit = tosdb_sstable_secondary_index_comparator(&last, &item);

    if(first_limit == 1 || last_limit == -1) {
        memory_free(st_idx);

        return true;
    }

    buffer_t buf_bf_in = buffer_encapsulate(st_idx->data + st_idx->minmax_key_size, st_idx->bloomfilter_size);
    buffer_t buf_bf_out = buffer_new_with_capacity(NULL, st_idx->bloomfilter_size * 2);

    uint64_t zc = zpack_unpack(buf_bf_in, buf_bf_out);

    buffer_destroy(buf_bf_in);

    if(!zc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot zunpack bf");
        memory_free(st_idx);
        buffer_destroy(buf_bf_out);

        return false;
    }

    uint64_t bf_data_len = 0;
    uint8_t* bf_data = buffer_get_all_bytes(buf_bf_out, &bf_data_len);

    buffer_destroy(buf_bf_out);

    data_t bf_tmp_d = {0};
    bf_tmp_d.type = DATA_TYPE_INT8_ARRAY;
    bf_tmp_d.length = bf_data_len;
    bf_tmp_d.value = bf_data;

    bloomfilter_t* bf = bloomfilter_deserialize(&bf_tmp_d);

    memory_free(bf_data);

    if(!bf) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot deserialize bloom filter");
        memory_free(st_idx);

        return false;
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
        bloomfilter_destroy(bf);
        memory_free(st_idx);

        return true;
    }

    bloomfilter_destroy(bf);


    buffer_t buf_idx_in = buffer_encapsulate(st_idx->data + st_idx->minmax_key_size + st_idx->bloomfilter_size, st_idx->index_size);
    buffer_t buf_idx_out = buffer_new_with_capacity(NULL, st_idx->index_size * 2);

    zc = zpack_unpack(buf_idx_in, buf_idx_out);

    buffer_destroy(buf_idx_in);

    if(!zc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot zunpack idx");
        memory_free(st_idx);
        buffer_destroy(buf_idx_out);

        return false;
    }

    uint8_t* idx_data = buffer_get_all_bytes(buf_idx_out, NULL);
    uint8_t* org_idx_data = idx_data;

    buffer_destroy(buf_idx_out);

    tosdb_memtable_secondary_index_item_t** st_idx_items = memory_malloc(sizeof(tosdb_memtable_index_item_t*) * (sli->record_count + 1));

    if(!st_idx_items) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");
        memory_free(st_idx);

        return false;
    }

    for(uint64_t i = 0; i < sli->record_count; i++) {
        st_idx_items[i] = (tosdb_memtable_secondary_index_item_t*)idx_data;

        idx_data += sizeof(tosdb_memtable_secondary_index_item_t) + st_idx_items[i]->secondary_key_length + st_idx_items[i]->primary_key_length;
    }

    tosdb_memtable_secondary_index_item_t** found_item = (tosdb_memtable_secondary_index_item_t**)binarysearch(st_idx_items,
                                                                                                               sli->record_count,
                                                                                                               sizeof(tosdb_memtable_index_item_t*),
                                                                                                               &item,
                                                                                                               tosdb_sstable_secondary_index_comparator);

    tosdb_memtable_secondary_index_item_t** org_found_item = found_item;

    boolean_t error = false;

    while(found_item) {
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

        res->is_deleted = s_idx_item->is_primary_key_deleted;
        res->key_hash = s_idx_item->primary_key_hash;
        res->key_length = s_idx_item->primary_key_length;
        memory_memcopy(s_idx_item->data + s_idx_item->secondary_key_length, res->key, res->key_length);

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

            res->is_deleted = s_idx_item->is_primary_key_deleted;
            res->key_hash = s_idx_item->primary_key_hash;
            res->key_length = s_idx_item->primary_key_length;
            memory_memcopy(s_idx_item->data + s_idx_item->secondary_key_length, res->key, res->key_length);

            if(!set_append(results, res)) {
                memory_free(res);
            }

            found_item--;
        }

    }


    memory_free(st_idx_items);
    memory_free(st_idx);
    memory_free(org_idx_data);

    return !error;
}

boolean_t tosdb_sstable_search_on_list(tosdb_record_t * record, set_t* results, linkedlist_t st_list, tosdb_memtable_secondary_index_item_t* item, uint64_t index_id) {
    boolean_t error = false;

    iterator_t* iter = linkedlist_iterator_create(st_list);

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

    if(map_size(ctx->keys) != 1) {
        PRINTLOG(TOSDB, LOG_ERROR, "record get supports only one key");

        return false;
    }

    iterator_t* iter = map_create_iterator(ctx->keys);

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
            linkedlist_t st_lvl_l = (linkedlist_t)map_get(ctx->table->sstable_levels, (void*)i);

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

