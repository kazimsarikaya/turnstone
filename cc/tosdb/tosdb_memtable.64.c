/**
 *
 * @file tosdb_memtable.64.c
 * @brief tosdb memtable interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <logging.h>
#include <bplustree.h>
#include <compression.h>
#include <strings.h>

MODULE("turnstone.kernel.db");

int8_t tosdb_memtable_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_index_item_t* ti1 = (tosdb_memtable_index_item_t*)i1;
    const tosdb_memtable_index_item_t* ti2 = (tosdb_memtable_index_item_t*)i2;

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

int8_t tosdb_memtable_secondary_index_comparator(const void* i1, const void* i2) {
    const tosdb_memtable_secondary_index_item_t* ti1 = (tosdb_memtable_secondary_index_item_t*)i1;
    const tosdb_memtable_secondary_index_item_t* ti2 = (tosdb_memtable_secondary_index_item_t*)i2;

    if(ti1->secondary_key_hash < ti2->secondary_key_hash) {
        return -1;
    }

    if(ti1->secondary_key_hash > ti2->secondary_key_hash) {
        return 1;
    }

    if(!ti1->secondary_key_length && !ti2->secondary_key_length) {
        return 0;
    }

    uint64_t min = MIN(ti1->secondary_key_length, ti2->secondary_key_length);

    int8_t res = memory_memcompare(ti1->data, ti2->data, min);

    if(min && res != 0) {
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
boolean_t tosdb_memtable_new(tosdb_table_t * tbl) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(!tbl->memtables) {
        tbl->memtables = linkedlist_create_stack();

        if(!tbl->memtables) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable stack for table %s", tbl->name);

            return false;
        }
    }

    tosdb_memtable_t* mt = memory_malloc(sizeof(tosdb_memtable_t));

    if(!mt) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable for table %s at memory", tbl->name);

        return false;
    }

    mt->is_dirty = true;
    mt->values = buffer_new_with_capacity(NULL, tbl->max_valuelog_size);

    if(!mt->values) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create valuelog for table %s at memory", tbl->name);
        memory_free(mt);

        return false;
    }

    mt->indexes = hashmap_integer(128);

    if(!mt->indexes) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable indexes for table %s at memory", tbl->name);
        buffer_destroy(mt->values);
        memory_free(mt);

        return false;
    }

    boolean_t error = false;

    iterator_t* iter = hashmap_iterator_create(tbl->indexes);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_index_t* index = (tosdb_index_t*)iter->get_item(iter);

        tosdb_memtable_index_t* mt_idx = memory_malloc(sizeof(tosdb_memtable_index_t));

        if(!mt_idx) {
            error = true;
            break;
        }

        mt_idx->ti = index;
        mt_idx->bloomfilter = bloomfilter_new(tbl->max_record_count, 0.1);

        if(!mt_idx->bloomfilter) {
            error = true;
            break;
        }

        boolean_t idx_unique = true;
        index_key_comparator_f cmp = tosdb_memtable_index_comparator;

        if(index->type == TOSDB_INDEX_SECONDARY) {
            idx_unique = false;
            cmp = tosdb_memtable_secondary_index_comparator;
        }

        mt_idx->index = bplustree_create_index_with_unique(32, cmp, idx_unique);

        if(!mt_idx->index) {
            error = true;
            break;
        }

        hashmap_put(mt->indexes, (void*)index->id, (void*)mt_idx);
        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(error) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot build memory index map for table %s", tbl->name);
        buffer_destroy(mt->values);

        iterator_t* mt_idx_iter = hashmap_iterator_create(mt->indexes);

        if(!mt_idx_iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index iterator for cleanup for table %s", tbl->name);
            hashmap_destroy(mt->indexes);
            buffer_destroy(mt->values);
            memory_free(mt);

            return false;
        }

        while(mt_idx_iter->end_of_iterator(mt_idx_iter) != 0) {
            tosdb_memtable_index_t* mt_idx = (tosdb_memtable_index_t*)mt_idx_iter->get_item(mt_idx_iter);

            bloomfilter_destroy(mt_idx->bloomfilter);

            if(mt_idx->index) {
                bplustree_destroy_index(mt_idx->index);
            }

            memory_free(mt_idx);

            mt_idx_iter = mt_idx_iter->next(mt_idx_iter);
        }

        mt_idx_iter->destroy(mt_idx_iter);

        hashmap_destroy(mt->indexes);
        memory_free(mt);

        return false;
    }


    mt->tbl = tbl;
    mt->id = tbl->memtable_next_id;
    tbl->memtable_next_id++;
    tbl->is_dirty = true;

    if(tbl->current_memtable) {
        tbl->current_memtable->is_full = true;
        tbl->current_memtable->is_readonly = true;
    }

    if(tbl->current_memtable && !tosdb_memtable_persist(tbl->current_memtable)) {
        return false;
    }

    tbl->current_memtable = mt;
    linkedlist_stack_push(tbl->memtables, mt);

    while(linkedlist_size(tbl->memtables) > tbl->max_memtable_count)  {
        tosdb_memtable_t* r_mt = (tosdb_memtable_t*)linkedlist_delete_at_tail(tbl->memtables);

        if(!tosdb_memtable_free(r_mt)) {
            error = true;
        }
    }

    return !error;
}
#pragma GCC diagnostic pop

boolean_t tosdb_memtable_free(tosdb_memtable_t* mt) {
    if(!mt) {
        return true;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "free memtable %lli for table %s has stli? %i has stlis? %i",
             mt->id, mt->tbl->name, mt->stli != NULL, mt->tbl->sstable_list_items != NULL);


    if(mt->stli) {
        linkedlist_stack_push(mt->tbl->sstable_list_items, mt->stli);
        PRINTLOG(TOSDB, LOG_DEBUG, "push sstable list item %p for table %s, stlis size %lli", mt->stli, mt->tbl->name, linkedlist_size(mt->tbl->sstable_list_items));
    }


    boolean_t error = false;

    iterator_t* mt_idx_iter = hashmap_iterator_create(mt->indexes);

    if(!mt_idx_iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index iterator for cleanup for table %s", mt->tbl->name);
        hashmap_destroy(mt->indexes);
        buffer_destroy(mt->values);
        memory_free(mt);

        return false;
    }

    while(mt_idx_iter->end_of_iterator(mt_idx_iter) != 0) {
        tosdb_memtable_index_t* mt_idx = (tosdb_memtable_index_t*)mt_idx_iter->get_item(mt_idx_iter);

        bloomfilter_destroy(mt_idx->bloomfilter);

        if(mt_idx->index) {
            iterator_t* iter = mt_idx->index->create_iterator(mt_idx->index);

            if(!iter) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");
                error = true;
            } else {
                while(iter->end_of_iterator(iter) != 0) {
                    void* item = (void*)iter->get_item(iter);

                    memory_free(item);

                    iter = iter->next(iter);
                }

                iter->destroy(iter);

            }

            bplustree_destroy_index(mt_idx->index);
        }

        memory_free(mt_idx);

        mt_idx_iter = mt_idx_iter->next(mt_idx_iter);
    }

    mt_idx_iter->destroy(mt_idx_iter);

    hashmap_destroy(mt->indexes);

    buffer_destroy(mt->values);
    memory_free(mt);

    return !error;
}

boolean_t tosdb_memtable_upsert(tosdb_record_t * record, boolean_t del) {
    if(!record || !record->context) {
        PRINTLOG(TOSDB, LOG_ERROR, "record is null");

        return false;
    }

    tosdb_record_context_t* r_ctx = record->context;
    tosdb_table_t* tbl = r_ctx->table;

    if(!tbl || !tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null or closed");

        return false;
    }

    lock_acquire(tbl->lock);

    if(!tbl->current_memtable || tbl->current_memtable->is_readonly) {
        if(!tosdb_memtable_new(tbl)) {
            lock_release(tbl->lock);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create a new memtable for table %s", tbl->name);

            return false;
        }
    }

    if(hashmap_size(tbl->indexes) != hashmap_size(r_ctx->keys)) {
        if(del) {
            if(!record->get_record(record)) {
                lock_release(tbl->lock);
                PRINTLOG(TOSDB, LOG_ERROR, "required columns are missing from record for table %s", tbl->name);

                return false;
            }
        } else {
            lock_release(tbl->lock);
            PRINTLOG(TOSDB, LOG_ERROR, "required columns are missing from record for table %s", tbl->name);

            return false;
        }
    }


    uint64_t offset = 0;
    uint64_t length = 0;

    if(!del) {
        data_t* sd = tosdb_record_serialize(record);

        if(!sd) {
            lock_release(tbl->lock);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot serialize record");

            return false;
        }

        if(buffer_get_length(tbl->current_memtable->values) > tbl->max_valuelog_size || tbl->current_memtable->record_count == tbl->max_record_count) {
            if(!tosdb_memtable_new(tbl)) {
                lock_release(tbl->lock);
                memory_free(sd->value);
                memory_free(sd);
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create a new memtable for table %s", tbl->name);

                return false;
            }

        }

        offset = buffer_get_position(tbl->current_memtable->values);
        length = sd->length;
        buffer_append_bytes(tbl->current_memtable->values, sd->value, sd->length);

        memory_free(sd->value);
        memory_free(sd);
    }

    boolean_t need_rc_inc = true;

    iterator_t* iter = hashmap_iterator_create(tbl->indexes);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_index_t* index = (tosdb_index_t*)iter->get_item(iter);

        tosdb_record_key_t* r_key = (tosdb_record_key_t*)hashmap_get(r_ctx->keys, (void*)index->id);

        if(!r_key) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot find record key for table %s", tbl->name);
            lock_release(tbl->lock);

            return false;
        }

        tosdb_memtable_index_t* mt_idx = (tosdb_memtable_index_t*)hashmap_get(tbl->current_memtable->indexes, (void*)index->id);

        if(!mt_idx) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot find memtable index for table %s", tbl->name);
            lock_release(tbl->lock);

            return false;
        }


        if(index->type != TOSDB_INDEX_SECONDARY) {
            tosdb_memtable_index_item_t* idx_item = memory_malloc(sizeof(tosdb_memtable_index_item_t) + r_key->key_length);

            if(!idx_item) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index item for table %s", tbl->name);
                lock_release(tbl->lock);

                return false;
            }

            idx_item->is_deleted = del;
            memory_memcopy(r_key->key, idx_item->key, r_key->key_length);
            idx_item->key_length = r_key->key_length;
            idx_item->key_hash = r_key->key_hash;
            idx_item->length = length;
            idx_item->offset = offset;

            uint8_t* u8_key = r_key->key;
            uint64_t u8_key_length = r_key->key_length;

            if(!u8_key_length) {
                u8_key_length = sizeof(uint64_t);
                u8_key = (uint8_t*)&r_key->key_hash;
            }

            data_t d_key = {0};
            d_key.type = DATA_TYPE_INT8_ARRAY;
            d_key.length = u8_key_length;
            d_key.value = u8_key;

            if(!bloomfilter_add(mt_idx->bloomfilter, &d_key)) {
                memory_free(idx_item);
                PRINTLOG(TOSDB, LOG_ERROR, "cannot add primary/unique index to bloomfilter for table %s", tbl->name);
                lock_release(tbl->lock);

                return false;
            }

            tosdb_memtable_index_item_t* old_item = NULL;

            mt_idx->index->insert(mt_idx->index, idx_item, idx_item, (void**)&old_item);

            if(old_item) {
                memory_free(old_item);
                need_rc_inc = false;
                PRINTLOG(TOSDB, LOG_ERROR, "duplicate primary/unique key for table %s mt id 0x%llx idx 0x%llx new key_hash 0x%llx old key_hash 0x%llx new data %s",
                         tbl->name, tbl->current_memtable->id, mt_idx->ti->id, idx_item->key_hash, old_item->key_hash, idx_item->key);
            }

        } else {
            const tosdb_record_key_t* pri_r_key = hashmap_get(r_ctx->keys, (void*)tbl->primary_index_id);
            uint64_t sec_idx_item_len = sizeof(tosdb_memtable_secondary_index_item_t) + r_key->key_length + pri_r_key->key_length;

            tosdb_memtable_secondary_index_item_t* sec_idx_item = memory_malloc(sec_idx_item_len);

            if(!sec_idx_item) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable secondary index item for table %s", tbl->name);
                lock_release(tbl->lock);

                return false;
            }

            sec_idx_item->secondary_key_hash = r_key->key_hash;
            sec_idx_item->secondary_key_length = r_key->key_length;
            sec_idx_item->primary_key_hash = pri_r_key->key_hash;
            sec_idx_item->primary_key_length = pri_r_key->key_length;
            sec_idx_item->is_primary_key_deleted = del;

            memory_memcopy(r_key->key, sec_idx_item->data, r_key->key_length);
            memory_memcopy(pri_r_key->key, sec_idx_item->data + r_key->key_length, pri_r_key->key_length);

            uint8_t* u8_key = r_key->key;
            uint64_t u8_key_length = r_key->key_length;

            if(!u8_key_length) {
                u8_key_length = sizeof(uint64_t);
                u8_key = (uint8_t*)&r_key->key_hash;
            }

            data_t d_key = {0};
            d_key.type = DATA_TYPE_INT8_ARRAY;
            d_key.length = u8_key_length;
            d_key.value = u8_key;

            if(!bloomfilter_add(mt_idx->bloomfilter, &d_key)) {
                memory_free(sec_idx_item);
                PRINTLOG(TOSDB, LOG_ERROR, "cannot add secondary index to bloomfilter for table %s", tbl->name);
                lock_release(tbl->lock);

                return false;
            }

            mt_idx->index->insert(mt_idx->index, sec_idx_item, sec_idx_item, NULL);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(need_rc_inc) {
        tbl->current_memtable->record_count++;
    }

    lock_release(tbl->lock);
    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_memtable_persist(tosdb_memtable_t* mt) {
    if(!mt) {
        PRINTLOG(TOSDB, LOG_ERROR, "memtable is null");

        return false;
    }

    if(!mt->is_dirty) {
        return true;
    }

    if(!mt->record_count) {
        return true;
    }

    boolean_t error = false;

    uint64_t valuelog_unpacked_size = buffer_get_length(mt->values);
    buffer_t* valuelog_out = buffer_new_with_capacity(NULL, valuelog_unpacked_size);

    buffer_seek(mt->values, 0, BUFFER_SEEK_DIRECTION_START);

    compression_t* compression = mt->tbl->db->tdb->compression;

    if(compression->pack(mt->values, valuelog_out) != 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot pack valuelog");
        buffer_destroy(valuelog_out);

        return false;
    }

    uint64_t len = buffer_get_length(valuelog_out);

    if(valuelog_unpacked_size && !len) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot pack valuelog");
        buffer_destroy(valuelog_out);

        return false;
    }

    uint64_t ol = 0;
    uint8_t* b_vl_data = buffer_get_all_bytes_and_destroy(valuelog_out, &ol);

    uint64_t b_vl_size = sizeof(tosdb_block_valuelog_t) + ol;

    if(b_vl_size % TOSDB_PAGE_SIZE) {
        b_vl_size += TOSDB_PAGE_SIZE - (b_vl_size % TOSDB_PAGE_SIZE);
    }

    tosdb_block_valuelog_t * b_vl = memory_malloc(b_vl_size);

    if(!b_vl) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create valuelog block");
        memory_free(b_vl_data);

        return false;
    }

    b_vl->header.block_size = b_vl_size;
    b_vl->header.block_type = TOSDB_BLOCK_TYPE_VALUELOG;
    b_vl->database_id = mt->tbl->db->id;
    b_vl->table_id = mt->tbl->id;
    b_vl->sstable_id = mt->id;
    b_vl->data_size = ol;
    b_vl->valuelog_unpacked_size = valuelog_unpacked_size;
    memory_memcopy(b_vl_data, b_vl->data, ol);
    memory_free(b_vl_data);

    uint64_t b_vl_loc = tosdb_block_write(mt->tbl->db->tdb, (tosdb_block_header_t*)b_vl);

    memory_free(b_vl);

    if(!b_vl_loc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot persist valuelog for memtable %lli of table %s", mt->id, mt->tbl->name);

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "valuelog for memtable %lli of table %s persisted at 0x%llx(0x%llx)", mt->id, mt->tbl->name, b_vl_loc, b_vl_size);

    uint64_t stli_size = sizeof(tosdb_block_sstable_list_item_t) + sizeof(tosdb_block_sstable_list_item_index_pair_t) * hashmap_size(mt->indexes);
    tosdb_block_sstable_list_item_t* stli = memory_malloc(stli_size);

    if(!stli) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list item");

        return false;
    }

    PRINTLOG(TOSDB, LOG_TRACE, "sstable list item for memtable %lli of table %s withc record count 0x%llx created", mt->id, mt->tbl->name, mt->record_count);

    stli->record_count = mt->record_count;
    stli->sstable_id = mt->id;
    stli->level = 1;
    stli->valuelog_location = b_vl_loc;
    stli->valuelog_size = b_vl_size;
    stli->index_count = hashmap_size(mt->indexes);

    uint64_t idx = 0;

    iterator_t* iter = hashmap_iterator_create(mt->indexes);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable index iterator");
        memory_free(stli);

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_memtable_index_t* mt_idx = (tosdb_memtable_index_t*)iter->get_item(iter);

        error |= !tosdb_memtable_index_persist(mt, stli, idx, mt_idx);

        idx++;

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(error) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot build memtable index list");
        memory_free(stli);

        return false;
    }

    if(!mt->tbl->sstable_list_items) {
        mt->tbl->sstable_list_items = linkedlist_create_stack();

        if(!mt->tbl->sstable_list_items) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list items stack");
            memory_free(stli);

            return false;
        }
    }

    mt->stli = stli;

    if(!error) {
        mt->is_dirty = false;
    }

    return !error;
}
#pragma GCC diagnostic pop

boolean_t tosdb_memtable_index_persist(tosdb_memtable_t* mt, tosdb_block_sstable_list_item_t* stli, uint64_t idx, tosdb_memtable_index_t* mt_idx) {
    if(!mt || !stli || !mt_idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "required params are null");

        return false;
    }

    data_t* bf_d = bloomfilter_serialize(mt_idx->bloomfilter);

    if(!bf_d) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot serialize bloom filter");

        return false;
    }

    buffer_t* buf_bf_in = buffer_encapsulate(bf_d->value, bf_d->length);

    if(!buf_bf_in) {
        memory_free(bf_d->value);
        memory_free(bf_d);

        return false;
    }

    buffer_t* buf_bf_out = buffer_new_with_capacity(NULL, bf_d->length);

    if(!buf_bf_out) {
        buffer_destroy(buf_bf_in);
        memory_free(bf_d->value);
        memory_free(bf_d);

        return false;
    }

    uint64_t bloomfilter_unpacked_size = bf_d->length;

    compression_t* compression = mt->tbl->db->tdb->compression;

    int8_t zc_res = compression->pack(buf_bf_in, buf_bf_out);

    uint64_t zc = buffer_get_length(buf_bf_out);

    data_free(bf_d);

    buffer_destroy(buf_bf_in);

    if(zc_res != 0 || !zc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot pack bloom filter");
        buffer_destroy(buf_bf_out);

        return false;
    }

    uint64_t bf_size = 0;
    uint8_t* bf_data = buffer_get_all_bytes_and_destroy(buf_bf_out, &bf_size);

    if(!bf_data) {
        return false;
    }

    void* first_key = NULL;
    uint64_t first_key_length = 0;
    void* last_key = NULL;
    uint64_t last_key_length = 0;

    buffer_t* buf_id_in = buffer_new();

    iterator_t* iter = mt_idx->index->create_iterator(mt_idx->index);

    if(mt_idx->ti->type != TOSDB_INDEX_SECONDARY) {
        while(iter->end_of_iterator(iter) != 0) {
            tosdb_memtable_index_item_t* ii = (tosdb_memtable_index_item_t*) iter->get_item(iter);

            if(!first_key) {
                first_key = ii;
                first_key_length = sizeof(tosdb_memtable_index_item_t) + ii->key_length;
            }

            last_key = ii;
            last_key_length = sizeof(tosdb_memtable_index_item_t) + ii->key_length;

            buffer_append_bytes(buf_id_in, (uint8_t*)ii, last_key_length);

            iter = iter->next(iter);
        }
    } else {
        while(iter->end_of_iterator(iter) != 0) {
            tosdb_memtable_secondary_index_item_t* ii = (tosdb_memtable_secondary_index_item_t*) iter->get_item(iter);

            if(!first_key) {
                first_key = ii;
                first_key_length = sizeof(tosdb_memtable_secondary_index_item_t) + ii->secondary_key_length + ii->primary_key_length;
            }

            last_key = ii;
            last_key_length = sizeof(tosdb_memtable_secondary_index_item_t) + ii->secondary_key_length + ii->primary_key_length;

            buffer_append_bytes(buf_id_in, (uint8_t*)ii, last_key_length);

            iter = iter->next(iter);
        }
    }

    iter->destroy(iter);

    if(!first_key || !last_key) {
        memory_free(bf_data);
        memory_free(buf_id_in);

        return false;
    }


    buffer_seek(buf_id_in, 0, BUFFER_SEEK_DIRECTION_START);

    uint64_t index_data_unpacked_size = buffer_get_length(buf_id_in);

    buffer_t* buf_id_out = buffer_new_with_capacity(NULL, index_data_unpacked_size);

    if(!buf_id_out) {
        memory_free(bf_data);
        buffer_destroy(buf_id_in);

        return false;
    }

    zc_res = compression->pack(buf_id_in, buf_id_out);

    zc = buffer_get_length(buf_id_out);

    buffer_destroy(buf_id_in);

    if(zc_res != 0 || !zc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot pack index data");
        buffer_destroy(buf_id_out);
        memory_free(bf_data);

        return false;
    }

    uint64_t index_size = 0;
    uint8_t* index_data = buffer_get_all_bytes_and_destroy(buf_id_out, &index_size);

    if(!index_data) {
        memory_free(bf_data);

        return false;
    }

    uint64_t idx_data_block_size = sizeof(tosdb_block_sstable_index_data_t) + index_size;

    if(idx_data_block_size % TOSDB_PAGE_SIZE) {
        idx_data_block_size += TOSDB_PAGE_SIZE - (idx_data_block_size % TOSDB_PAGE_SIZE);
    }

    tosdb_block_sstable_index_data_t* b_sid = memory_malloc(idx_data_block_size);

    if(!b_sid) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable index data block");
        memory_free(bf_data);
        memory_free(index_data);

        return false;
    }

    b_sid->header.block_size = idx_data_block_size;
    b_sid->header.block_type = TOSDB_BLOCK_TYPE_SSTABLE_INDEX_DATA;

    b_sid->database_id = mt->tbl->db->id;
    b_sid->table_id = mt->tbl->id;
    b_sid->sstable_id = mt->id;
    b_sid->index_id = mt_idx->ti->id;
    b_sid->index_data_size = index_size;
    b_sid->index_data_unpacked_size = index_data_unpacked_size;

    memory_memcopy(index_data, b_sid->data, index_size);
    memory_free(index_data);

    uint64_t idx_data_block_loc = tosdb_block_write(mt->tbl->db->tdb, (tosdb_block_header_t*)b_sid);

    memory_free(b_sid);

    if(!idx_data_block_loc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write sstable index data block");
        memory_free(bf_data);

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "data index %lli of memtable %lli of table %s persisted at 0x%llx(0x%llx)", mt_idx->ti->id, mt->id, mt->tbl->name, idx_data_block_loc, idx_data_block_size);

    uint64_t minmax_key_size = first_key_length + last_key_length;
    uint64_t block_size = sizeof(tosdb_block_sstable_index_t) + minmax_key_size + bf_size;

    if(block_size % TOSDB_PAGE_SIZE) {
        block_size += TOSDB_PAGE_SIZE - (block_size % TOSDB_PAGE_SIZE);
    }

    tosdb_block_sstable_index_t* b_si = memory_malloc(block_size);

    if(!b_si) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable index block");
        memory_free(bf_data);

        return false;
    }

    b_si->header.block_size = block_size;
    b_si->header.block_type = TOSDB_BLOCK_TYPE_SSTABLE_INDEX;

    b_si->database_id = mt->tbl->db->id;
    b_si->table_id = mt->tbl->id;
    b_si->sstable_id = mt->id;
    b_si->index_id = mt_idx->ti->id;
    b_si->minmax_key_size = minmax_key_size;
    b_si->bloomfilter_size = bf_size;
    b_si->bloomfilter_unpacked_size = bloomfilter_unpacked_size;
    b_si->index_data_size = idx_data_block_size;
    b_si->index_data_location = idx_data_block_loc;

    uint8_t* tmp = &b_si->data[0];

    memory_memcopy(first_key, tmp, first_key_length);
    tmp += first_key_length;
    memory_memcopy(last_key, tmp, last_key_length);
    tmp += last_key_length;
    memory_memcopy(bf_data, tmp, bf_size);
    memory_free(bf_data);

    uint64_t block_loc = tosdb_block_write(mt->tbl->db->tdb, (tosdb_block_header_t*)b_si);

    memory_free(b_si);

    if(!block_loc) {
        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "index %lli of memtable %lli of table %s persisted at 0x%llx(0x%llx)", mt_idx->ti->id, mt->id, mt->tbl->name, block_loc, block_size);

    stli->indexes[idx].index_id = mt_idx->ti->id;
    stli->indexes[idx].index_location = block_loc;
    stli->indexes[idx].index_size = block_size;

    return true;
}

boolean_t tosdb_memtable_is_deleted(tosdb_record_t* record) {
    if(!record || !record->context) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

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
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable intex item");

        return false;
    }

    item->key_hash = r_key->key_hash;
    item->key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->key, item->key_length);

    linkedlist_t* mts = ctx->table->memtables;

    if(!mts) {
        memory_free(item);

        return false;
    }

    boolean_t found = false;
    const tosdb_memtable_index_item_t* found_item = NULL;
    const tosdb_memtable_t* mt = NULL;

    iter = linkedlist_iterator_create(mts);

    while(iter->end_of_iterator(iter) != 0) {
        mt = iter->get_item(iter);

        const tosdb_memtable_index_t* mt_idx = hashmap_get(mt->indexes, (void*)r_key->index_id);

        iterator_t* s_iter = mt_idx->index->search(mt_idx->index, item, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

        if(s_iter->end_of_iterator(s_iter) != 0) {
            found_item = s_iter->get_item(s_iter);
            found = true;
            s_iter->destroy(s_iter);

            break;
        }

        s_iter->destroy(s_iter);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    memory_free(item);

    if(!found) {
        return false;
    }

    if(!found_item) {
        return false;
    }

    if(found_item->is_deleted) {
        return true;
    }

    return false;
}

boolean_t tosdb_memtable_get(tosdb_record_t* record) {
    if(!record || !record->context) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

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
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable intex item");

        return false;
    }

    item->key_hash = r_key->key_hash;
    item->key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->key, item->key_length);

    linkedlist_t* mts = ctx->table->memtables;

    if(linkedlist_size(mts) == 0) {
        memory_free(item);

        return false;
    }

    boolean_t found = false;
    const tosdb_memtable_index_item_t* found_item = NULL;
    const tosdb_memtable_t* mt = NULL;

    iter = linkedlist_iterator_create(mts);

    uint64_t col_id = 0;

    while(iter->end_of_iterator(iter) != 0) {
        mt = iter->get_item(iter);

        const tosdb_memtable_index_t* mt_idx = hashmap_get(mt->indexes, (void*)r_key->index_id);

        col_id = mt_idx->ti->column_id;

        iterator_t* s_iter = mt_idx->index->search(mt_idx->index, item, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

        if(s_iter->end_of_iterator(s_iter) != 0) {
            found_item = s_iter->get_item(s_iter);
            found = true;
            s_iter->destroy(s_iter);

            break;
        }

        s_iter->destroy(s_iter);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    memory_free(item);

    if(!found) {
        return false;
    }

    if(!found_item) {
        return false;
    }

    if(found_item->is_deleted) {
        return false;
    }

    lock_acquire(mt->tbl->lock);
    uint64_t old_pos = buffer_get_position(mt->values);
    buffer_seek(mt->values, found_item->offset, BUFFER_SEEK_DIRECTION_START);
    uint8_t* f_d = buffer_get_bytes(mt->values, found_item->length);
    buffer_seek(mt->values, old_pos, BUFFER_SEEK_DIRECTION_START);
    lock_release(mt->tbl->lock);

    data_t s_d = {0};
    s_d.length = found_item->length;
    s_d.type = DATA_TYPE_INT8_ARRAY;
    s_d.value = f_d;

    data_t* r_d = data_bson_deserialize(&s_d, DATA_SERIALIZE_WITH_ALL);

    memory_free(f_d);

    if(!r_d) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot deserialize data");

        return false;
    }


    data_t* tmp = r_d->value;

    for(uint64_t i = 0; i < r_d->length; i++) {
        uint64_t tmp_col_id = (uint64_t)tmp[i].name->value;

        if(tmp_col_id == col_id) {
            continue;
        }

        if(!tosdb_record_set_data_with_colid(record, tmp_col_id, tmp[i].type, tmp[i].length, tmp[i].value)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot populate record");
        }
    }

    data_free(r_d);

    return found;
}

boolean_t tosdb_memtable_search(tosdb_record_t* record, set_t* results) {
    if(!record || !record->context) {
        return false;
    }

    tosdb_record_context_t* ctx = record->context;

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

    tosdb_memtable_secondary_index_item_t* item = memory_malloc(sizeof(tosdb_memtable_secondary_index_item_t) + r_key->key_length);

    if(!item) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable secondary index item");

        return false;
    }

    item->secondary_key_hash = r_key->key_hash;
    item->secondary_key_length = r_key->key_length;
    memory_memcopy(r_key->key, item->data, item->secondary_key_length);

    linkedlist_t* mts = ctx->table->memtables;

    if(linkedlist_size(mts) == 0) {
        memory_free(item);

        return true;
    }

    const tosdb_memtable_t* mt = NULL;
    boolean_t error = false;

    iter = linkedlist_iterator_create(mts);

    while(iter->end_of_iterator(iter) != 0) {
        mt = iter->get_item(iter);

        const tosdb_memtable_index_t* mt_idx = hashmap_get(mt->indexes, (void*)r_key->index_id);

        iterator_t* s_iter = mt_idx->index->search(mt_idx->index, item, NULL, INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL);

        while(s_iter->end_of_iterator(s_iter) != 0) {
            const tosdb_memtable_secondary_index_item_t* s_idx_item = s_iter->get_item(s_iter);

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

            s_iter = s_iter->next(s_iter);
        }

        s_iter->destroy(s_iter);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    memory_free(item);


    return !error;
}

