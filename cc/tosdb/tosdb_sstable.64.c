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

boolean_t tosdb_sstable_get_on_list(tosdb_record_t * record, linkedlist_t st_list, tosdb_memtable_index_item_t* item, uint64_t index_id);
boolean_t tosdb_sstable_get_on_index(tosdb_record_t * record, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_index_item_t* item, uint64_t index_id);
int8_t    tosdb_sstable_index_comparator(const void* i1, const void* i2);

int8_t tosdb_sstable_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_index_item_t* ti1 = (tosdb_memtable_index_item_t*)*((void**)i1);
    const tosdb_memtable_index_item_t* ti2 = (tosdb_memtable_index_item_t*)*((void**)i2);

    if(ti1->key_hash < ti2->key_hash) {
        return -1;
    }

    if(ti1->key_hash > ti2->key_hash) {
        return 1;
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


boolean_t tosdb_sstable_get_on_index(tosdb_record_t * record, tosdb_block_sstable_list_item_t* sli, tosdb_memtable_index_item_t* item, uint64_t index_id){
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

    buffer_t buf_bf_in = buffer_encapsulate(st_idx->data, st_idx->bloomfilter_size);
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

    data_t item_tmp_data = {0};
    item_tmp_data.type = DATA_TYPE_INT8_ARRAY;
    item_tmp_data.length = item->key_length;
    item_tmp_data.value = item->key;

    if(!bloomfilter_check(bf, &item_tmp_data)) {
        bloomfilter_destroy(bf);
        memory_free(st_idx);

        return false;
    }

    bloomfilter_destroy(bf);


    buffer_t buf_idx_in = buffer_encapsulate(&st_idx->data[st_idx->bloomfilter_size], st_idx->index_size);
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

    tosdb_memtable_index_item_t** st_idx_items = memory_malloc(sizeof(tosdb_memtable_index_item_t*) * sli->record_count);

    if(!st_idx_items) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index item array");
        memory_free(st_idx);

        return false;
    }

    for(uint64_t i = 0; i < sli->record_count; i++) {
        st_idx_items[i] = (tosdb_memtable_index_item_t*)idx_data;

        idx_data += sizeof(tosdb_memtable_index_item_t) + st_idx_items[i]->key_length;
    }

    tosdb_memtable_index_item_t* found_item = *(tosdb_memtable_index_item_t**)binarsearch(st_idx_items,
                                                                                          sli->record_count,
                                                                                          sizeof(tosdb_memtable_index_item_t*),
                                                                                          &item,
                                                                                          tosdb_sstable_index_comparator);


    if(!found_item) {
        memory_free(st_idx_items);
        memory_free(st_idx);
        memory_free(org_idx_data);

        return false;
    }

    if(found_item->is_deleted) {
        memory_free(st_idx_items);
        memory_free(st_idx);
        memory_free(org_idx_data);

        return false;
    }

    memory_free(st_idx_items);

    uint64_t offset = found_item->offset;
    uint64_t length = found_item->length;

    memory_free(org_idx_data);
    memory_free(st_idx);

    tosdb_block_valuelog_t* b_vl = (tosdb_block_valuelog_t*)tosdb_block_read(ctx->table->db->tdb, sli->valuelog_location, sli->valuelog_size);

    if(!b_vl) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read valuelog block");

        return false;
    }

    buffer_t buf_vl_in = buffer_encapsulate(b_vl->data, b_vl->data_size);
    buffer_t buf_vl_out = buffer_new_with_capacity(NULL, b_vl->data_size * 2);

    zc = zpack_unpack(buf_vl_in, buf_vl_out);

    memory_free(b_vl);
    buffer_destroy(buf_vl_in);

    if(!zc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot zunpack valuelog");
        buffer_destroy(buf_idx_out);

        return false;
    }

    buffer_seek(buf_vl_out, offset, BUFFER_SEEK_DIRECTION_START);
    uint8_t* value_data = buffer_get_bytes(buf_vl_out, length);
    buffer_destroy(buf_vl_out);

    if(!value_data) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read value data from valuelog");

        return false;
    }

    data_t s_d = {0};
    s_d.length = length;
    s_d.type = DATA_TYPE_INT8_ARRAY;
    s_d.value = value_data;

    data_t* r_d = data_bson_deserialize(&s_d, DATA_SERIALIZE_WITH_ALL);

    memory_free(value_data);

    if(!r_d) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot deserialize data");

        return false;
    }

    tosdb_index_t* idx = (tosdb_index_t*)map_get(ctx->table->indexes, (void*)index_id);

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

    data_free(r_d);

    return true;
}

boolean_t tosdb_sstable_get_on_list(tosdb_record_t * record, linkedlist_t st_list, tosdb_memtable_index_item_t* item, uint64_t index_id) {
    boolean_t found = false;

    iterator_t* iter = linkedlist_iterator_create(st_list);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstables list items iterator");

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_block_sstable_list_item_t* sli = (tosdb_block_sstable_list_item_t*) iter->get_item(iter);

        if(index_id <= sli->index_count) {
            if(tosdb_sstable_get_on_index(record, sli, item, index_id)) {
                found = true;

                break;
            }
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

    tosdb_memtable_index_item_t* item = memory_malloc(sizeof(tosdb_memtable_index_item_t) + r_key->key_length);

    item->key_hash = r_key->key_hash;
    item->key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->key, item->key_length);

    if(ctx->table->sstable_list_items && tosdb_sstable_get_on_list(record, ctx->table->sstable_list_items, item, r_key->index_id)) {
        memory_free(item);

        return true;
    }


    if(ctx->table->sstable_levels) {
        for(uint64_t i = 1; i <= ctx->table->sstable_max_level; i++) {
            linkedlist_t st_lvl_l = (linkedlist_t)map_get(ctx->table->sstable_levels, (void*)i);

            if(st_lvl_l) {
                if(tosdb_sstable_get_on_list(record, st_lvl_l, item, r_key->index_id)) {
                    memory_free(item);

                    return true;
                }
            }
        }
    }

    memory_free(item);

    return false;
}

