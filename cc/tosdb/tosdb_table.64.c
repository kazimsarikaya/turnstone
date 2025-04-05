/**
 * @file tosdb_table.64.c
 * @brief tosdb table interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.kernel.db");


const tosdb_column_t* tosdb_table_get_column_by_index_id(tosdb_table_t* tbl, uint64_t id) {
    if(!tbl || !tbl->columns || !tbl->indexes) {
        PRINTLOG(TOSDB, LOG_ERROR, "table/coumns/indexes is null");

        return NULL;
    }

    tosdb_index_t* idx = (tosdb_index_t*)hashmap_get(tbl->indexes, (void*)id);

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot find index with id %lli", id);

        return NULL;
    }

    iterator_t* it = hashmap_iterator_create(tbl->columns);

    if(!it) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator for columns");

        return NULL;
    }

    const tosdb_column_t* col = NULL;

    while(it->end_of_iterator(it) != 0) {
        const tosdb_column_t* t_col = (const tosdb_column_t*)it->get_item(it);

        if(idx->column_id == t_col->id) {
            col = t_col;

            break;
        }

        it = it->next(it);
    }

    it->destroy(it);

    return col;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_table_load_sstables(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return false;
    }

    if(!tbl->sstable_list_location) {
        PRINTLOG(TOSDB, LOG_DEBUG, "no sstable for table %s", tbl->name);

        return true;
    }

    if(!tbl->sstable_levels) {
        tbl->sstable_levels = hashmap_integer(128);

        if(!tbl->sstable_levels) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable levels map");

            return false;
        }
    }

    uint64_t st_list_loc = tbl->sstable_list_location;
    uint64_t st_list_size = tbl->sstable_list_size;

    while(st_list_loc != 0) {
        tosdb_block_sstable_list_t* st_list = (tosdb_block_sstable_list_t*)tosdb_block_read(tbl->db->tdb, st_list_loc, st_list_size);

        if(!st_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read sstable list for table %s", tbl->name);

            return false;
        }

        uint8_t* st_list_data = (uint8_t*)&st_list->sstables[0];

        for(uint64_t i = 0; i < st_list->sstable_count; i++) {
            tosdb_block_sstable_list_item_t* t_st = (tosdb_block_sstable_list_item_t*)st_list_data;

            uint64_t st_size = sizeof(tosdb_block_sstable_list_item_t);
            st_size += sizeof(tosdb_block_sstable_list_item_index_pair_t) * t_st->index_count;

            tosdb_block_sstable_list_item_t* st = memory_malloc(st_size);

            if(!st) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list for level");
                memory_free(st_list);

                return false;
            }

            memory_memcopy(st_list_data, st, st_size);
            st_list_data += st_size;

            uint64_t level = st->level;
            tbl->sstable_max_level = MAX(tbl->sstable_max_level, level);


            list_t* st_l = (list_t*)hashmap_get(tbl->sstable_levels, (void*)level);

            if(!st_l) {
                st_l = list_create_queue();

                if(!st_l) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list for level");
                    memory_free(st_list);
                    memory_free(st);

                    return false;
                }

                hashmap_put(tbl->sstable_levels, (void*)level, st_l);
            }

            list_queue_push(st_l, st);

            PRINTLOG(TOSDB, LOG_DEBUG, "table %s sstable %lli with level %lli lazy loaded", tbl->name, st->sstable_id, st->level);
        }

        if(st_list->header.previous_block_invalid) {
            memory_free(st_list);

            break;
        }

        st_list_loc = st_list->header.previous_block_location;
        st_list_size = st_list->header.previous_block_size;

        memory_free(st_list);

    }

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s max sstable level value %lli map size %lli", tbl->name, tbl->sstable_max_level, hashmap_size(tbl->sstable_levels));

    return true;
}

boolean_t tosdb_table_load_indexes(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return false;
    }

    tbl->indexes = hashmap_integer(128);
    tbl->index_column_map = hashmap_integer(128);

    if(!tbl->indexes) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index map for table %s", tbl->name);

        return false;
    }

    uint64_t idx_list_loc = tbl->index_list_location;
    uint64_t idx_list_size = tbl->index_list_size;

    while(idx_list_loc != 0) {
        tosdb_block_index_list_t* idx_list = (tosdb_block_index_list_t*)tosdb_block_read(tbl->db->tdb, idx_list_loc, idx_list_size);

        if(!idx_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read index list for table %s", tbl->name);

            return false;
        }

        for(uint64_t i = 0; i < idx_list->index_count; i++) {

            if(hashmap_exists(tbl->indexes, (void*)idx_list->indexes[i].id)) {
                continue;
            }

            tosdb_index_t* idx = memory_malloc(sizeof(tosdb_index_t));

            if(!idx) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate idx for table %s", tbl->name);
                memory_free(idx_list);

                return false;
            }

            idx->id = idx_list->indexes[i].id;
            idx->is_deleted = idx_list->indexes[i].deleted;
            idx->type = idx_list->indexes[i].type;
            idx->column_id = idx_list->indexes[i].column_id;

            hashmap_put(tbl->indexes, (void*)idx->id, idx);
            hashmap_put(tbl->index_column_map, (void*)idx->column_id, idx);
        }


        if(idx_list->header.previous_block_invalid) {
            memory_free(idx_list);

            break;
        }

        idx_list_loc = idx_list->header.previous_block_location;
        idx_list_size = idx_list->header.previous_block_size;

        memory_free(idx_list);
    }

    return true;
}

boolean_t tosdb_table_load_columns(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return false;
    }

    tbl->columns = hashmap_string(128);

    if(!tbl->columns) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column map for table %s", tbl->name);

        return false;
    }

    uint64_t col_list_loc = tbl->column_list_location;
    uint64_t col_list_size = tbl->column_list_size;

    while(col_list_loc != 0) {
        tosdb_block_column_list_t* col_list = (tosdb_block_column_list_t*)tosdb_block_read(tbl->db->tdb, col_list_loc, col_list_size);

        if(!col_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read column list for table %s", tbl->name);

            return false;
        }

        char_t name_buf[TOSDB_NAME_MAX_LEN + 1] = {0};

        for(uint64_t i = 0; i < col_list->column_count; i++) {
            memory_memclean(name_buf, TOSDB_NAME_MAX_LEN + 1);
            memory_memcopy(col_list->columns[i].name, name_buf, TOSDB_NAME_MAX_LEN);

            if(hashmap_exists(tbl->columns, name_buf)) {
                continue;
            }

            tosdb_column_t* col = memory_malloc(sizeof(tosdb_column_t));

            if(!col) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate col for table %s", tbl->name);
                memory_free(col_list);

                return false;
            }

            col->id = col_list->columns[i].id;
            col->name = strdup(name_buf);
            col->is_deleted = col_list->columns[i].deleted;
            col->type = col_list->columns[i].type;

            hashmap_put(tbl->columns, col->name, col);
        }


        if(col_list->header.previous_block_invalid) {
            memory_free(col_list);

            break;
        }

        col_list_loc = col_list->header.previous_block_location;
        col_list_size = col_list->header.previous_block_size;

        memory_free(col_list);
    }

    return true;
}
#pragma GCC diagnostic pop

tosdb_table_t* tosdb_table_load_table(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return NULL;
    }

    if(tbl->is_deleted) {
        PRINTLOG(TOSDB, LOG_WARNING, "tbl is deleted");
        return NULL;
    }

    if(tbl->is_open) {
        return tbl;
    }

    if(!tbl->metadata_location || !tbl->metadata_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "metadata not found for %s", tbl->name);

        return NULL;
    }

    tosdb_block_table_t* tbl_block = (tosdb_block_table_t*)tosdb_block_read(tbl->db->tdb, tbl->metadata_location, tbl->metadata_size);

    if(!tbl_block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read table %s metadata", tbl->name);

        return NULL;
    }

    tbl->column_list_location = tbl_block->column_list_location;
    tbl->column_list_size = tbl_block->column_list_size;
    tbl->column_next_id = tbl_block->column_next_id;

    if(!tosdb_table_load_columns(tbl)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load columns of table %s", tbl->name);
    }

    tbl->index_list_location = tbl_block->index_list_location;
    tbl->index_list_size = tbl_block->index_list_size;
    tbl->index_next_id = tbl_block->index_next_id;

    if(!tosdb_table_load_indexes(tbl)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load indexes of table %s", tbl->name);
    }

    tbl->primary_column_id = tbl_block->primary_column_id;
    tbl->primary_index_id = tbl_block->primary_index_id;
    tbl->primary_column_type = tbl_block->primary_column_type;
    tbl->compaction_index_id_hint = tbl_block->compaction_index_id_hint;

    tbl->memtable_next_id = tbl_block->memtable_next_id;
    tbl->sstable_list_location = tbl_block->sstable_list_location;
    tbl->sstable_list_size = tbl_block->sstable_list_size;

    if(!tosdb_table_load_sstables(tbl)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load lazy load sstables of table %s", tbl->name);
    }

    memory_free(tbl_block);

    tbl->is_open = true;

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s loaded", tbl->name);

    return tbl;
}

tosdb_table_t* tosdb_table_create_or_open(tosdb_database_t* db, const char_t* name, uint64_t max_record_count, uint64_t max_valuelog_size, uint64_t max_memtable_count) {
    if(strlen(name) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "table name cannot be longer than %i", TOSDB_NAME_MAX_LEN);
        return NULL;
    }

    if(!db) {
        PRINTLOG(TOSDB, LOG_ERROR, "sdb is null");

        return NULL;
    }


    if(hashmap_exists(db->tables, name)) {
        tosdb_table_t* tbl = (tosdb_table_t*)hashmap_get(db->tables, name);

        if(tbl->is_deleted) {
            PRINTLOG(TOSDB, LOG_ERROR, "table %s was deleted", tbl->name);

            return NULL;
        }

        if(tbl->is_open) {
            PRINTLOG(TOSDB, LOG_DEBUG, "table %s will be returned", tbl->name);

            return tbl;
        }

        tbl->max_record_count = max_record_count;
        tbl->max_valuelog_size = max_valuelog_size;
        tbl->max_memtable_count = max_memtable_count;

        PRINTLOG(TOSDB, LOG_DEBUG, "table %s will be lazy loaded", tbl->name);

        return tosdb_table_load_table(tbl);
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s not found, new one will be created", name);

    if(!db->table_new) {
        db->table_new = hashmap_integer(128);

        if(!db->table_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new table list");

            return NULL;
        }
    }

    lock_acquire(db->lock);

    tosdb_table_t* tbl = memory_malloc(sizeof(tosdb_table_t));

    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create table struct");

        lock_release(db->lock);

        return NULL;
    }


    tbl->id = db->table_next_id;

    db->table_next_id++;
    db->is_dirty = true;

    tbl->db = db;
    tbl->name = strdup(name);

    tbl->is_open = true;
    tbl->is_dirty = true;

    tbl->column_next_id = 1;
    tbl->columns = hashmap_string(128);

    tbl->index_next_id = 1;
    tbl->indexes = hashmap_integer(128);
    tbl->index_column_map = hashmap_integer(128);

    tbl->memtable_next_id = 1;

    tbl->max_record_count = max_record_count;
    tbl->max_valuelog_size = max_valuelog_size;
    tbl->max_memtable_count = max_memtable_count;

    tbl->compaction_index_id_hint = -1ULL;


    hashmap_put(db->tables, name, tbl);

    hashmap_put(db->table_new, (void*)tbl->id, tbl);

    lock_release(db->lock);

    PRINTLOG(TOSDB, LOG_DEBUG, "new table %s created", tbl->name);

    return tbl;
}

boolean_t tosdb_table_close(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }

    boolean_t error = false;

    if(tbl->is_open) {
        PRINTLOG(TOSDB, LOG_DEBUG, "table %s will be closed", tbl->name);

        if(tbl->is_dirty) {
            if(!tosdb_table_persist(tbl)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot persist table %s", tbl->name);
                error = true;
            } else {
                PRINTLOG(TOSDB, LOG_DEBUG, "table %s persisted", tbl->name);
            }
        }

        iterator_t* iter = hashmap_iterator_create(tbl->columns);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create column iterator");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_column_t* col = (tosdb_column_t*)iter->get_item(iter);

            memory_free(col->name);

            memory_free(col);

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        hashmap_destroy(tbl->columns);
        tbl->columns = NULL;

        PRINTLOG(TOSDB, LOG_TRACE, "columns of table %s destroyed", tbl->name);

        iter = hashmap_iterator_create(tbl->indexes);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_index_t* idx = (tosdb_index_t*)iter->get_item(iter);

            memory_free(idx);

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        hashmap_destroy(tbl->indexes);
        hashmap_destroy(tbl->index_column_map);
        tbl->indexes = NULL;
        tbl->index_column_map = NULL;

        PRINTLOG(TOSDB, LOG_TRACE, "indexes of table %s destroyed", tbl->name);

        if(tbl->memtables) {
            iter = list_iterator_create(tbl->memtables);

            if(!iter) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable iterator");

                return false;
            }

            while(iter->end_of_iterator(iter) != 0) {
                tosdb_memtable_t* mt = (tosdb_memtable_t*)iter->delete_item(iter);

                if(!tosdb_memtable_free(mt)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot free memtable for table %s", tbl->name);
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);

            list_destroy(tbl->memtables);
            tbl->memtables = NULL;
            tbl->current_memtable = NULL;
        }

        PRINTLOG(TOSDB, LOG_TRACE, "memtables of table %s destroyed", tbl->name);

        if(tbl->sstable_levels) {
            iterator_t* stl_iter = hashmap_iterator_create(tbl->sstable_levels);

            if(!stl_iter) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable levels iterator");

                return false;
            }

            while(stl_iter->end_of_iterator(stl_iter) != 0) {
                list_t* st_list = (list_t*)stl_iter->get_item(stl_iter);

                list_destroy_with_data(st_list);

                stl_iter = stl_iter->next(stl_iter);
            }

            stl_iter->destroy(stl_iter);

            hashmap_destroy(tbl->sstable_levels);
            tbl->sstable_levels = NULL;

            PRINTLOG(TOSDB, LOG_TRACE, "sstable levels of table %s destroyed", tbl->name);
        }

        tbl->is_open = false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s is closed", tbl->name);

    return !error;
}

boolean_t tosdb_table_free(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }

    boolean_t error = false;

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s will be freed", tbl->name);

    if(tbl->columns) {
        iterator_t* iter = hashmap_iterator_create(tbl->columns);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create column iterator");
            error = true;
        } else {
            while(iter->end_of_iterator(iter) != 0) {
                tosdb_column_t* col = (tosdb_column_t*)iter->get_item(iter);

                memory_free(col->name);

                memory_free(col);

                iter = iter->next(iter);
            }

            iter->destroy(iter);
        }

        hashmap_destroy(tbl->columns);
    }

    if(tbl->indexes) {
        iterator_t* iter = hashmap_iterator_create(tbl->indexes);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");

            error = true;
        } else {
            while(iter->end_of_iterator(iter) != 0) {
                tosdb_index_t* idx = (tosdb_index_t*)iter->get_item(iter);

                memory_free(idx);

                iter = iter->next(iter);
            }

            iter->destroy(iter);

        }

        hashmap_destroy(tbl->indexes);
        hashmap_destroy(tbl->index_column_map);
    }

    if(tbl->memtables) {
        iterator_t* iter = list_iterator_create(tbl->memtables);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create memtable iterator");

            error = true;
        } else {
            while(iter->end_of_iterator(iter) != 0) {
                tosdb_memtable_t* mt = (tosdb_memtable_t*)iter->get_item(iter);

                if(!tosdb_memtable_free(mt)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot free memtable for table %s", tbl->name);

                    error = true;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);

        }

        list_destroy(tbl->memtables);
    }

    if(tbl->sstable_list_items) {
        list_destroy_with_data(tbl->sstable_list_items);
    }

    if(tbl->sstable_levels) {
        iterator_t* stl_iter = hashmap_iterator_create(tbl->sstable_levels);

        if(!stl_iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable levels iterator");

            error = true;
        } else {
            while(stl_iter->end_of_iterator(stl_iter) != 0) {
                list_t* st_list = (list_t*)stl_iter->get_item(stl_iter);

                list_destroy_with_data(st_list);

                stl_iter = stl_iter->next(stl_iter);
            }

            stl_iter->destroy(stl_iter);

        }

        hashmap_destroy(tbl->sstable_levels);
        tbl->sstable_levels = NULL;
    }

    memory_free(tbl->name);
    lock_destroy(tbl->lock);
    memory_free(tbl);
    PRINTLOG(TOSDB, LOG_DEBUG, "table freed");

    return !error;
}

boolean_t tosdb_table_index_persist(tosdb_table_t* tbl) {
    uint64_t metadata_size = sizeof(tosdb_block_index_list_t) + sizeof(tosdb_block_index_list_item_t) * tbl->index_new_count;

    if (metadata_size % TOSDB_PAGE_SIZE) {
        metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));
    }

    tosdb_block_index_list_t* block = memory_malloc(metadata_size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index list");

        return false;
    }

    block->header.block_type = TOSDB_BLOCK_TYPE_INDEX_LIST;
    block->header.block_size = metadata_size;
    block->header.previous_block_location = tbl->index_list_location;
    block->header.previous_block_size = tbl->index_list_size;

    block->database_id = tbl->db->id;
    block->table_id = tbl->id;

    iterator_t* iter = list_iterator_create(tbl->index_new);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");

        memory_free(block);

        return false;
    }

    block->index_count = tbl->index_new_count;

    uint64_t idx_idx = 0;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_index_t* idx = (tosdb_index_t*)iter->delete_item(iter);

        block->indexes[idx_idx].id = idx->id;
        block->indexes[idx_idx].column_id = idx->column_id;
        block->indexes[idx_idx].deleted = idx->is_deleted;
        block->indexes[idx_idx].type = idx->type;

        iter = iter->next(iter);

        idx_idx++;
    }

    iter->destroy(iter);

    uint64_t loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

    if(loc == 0) {
        memory_free(block);

        return false;
    }

    tbl->index_list_location = loc;
    tbl->index_list_size = block->header.block_size;

    memory_free(block);


    tbl->index_new_count = 0;
    list_destroy(tbl->index_new);

    return true;
}

boolean_t tosdb_table_column_persist(tosdb_table_t* tbl) {
    uint64_t metadata_size = sizeof(tosdb_block_column_list_t) + sizeof(tosdb_block_column_list_item_t) * tbl->column_new_count;

    if (metadata_size % TOSDB_PAGE_SIZE) {
        metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));
    }

    tosdb_block_column_list_t* block = memory_malloc(metadata_size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column list");

        return false;
    }

    block->header.block_type = TOSDB_BLOCK_TYPE_COLUMN_LIST;
    block->header.block_size = metadata_size;
    block->header.previous_block_location = tbl->column_list_location;
    block->header.previous_block_size = tbl->column_list_size;

    block->database_id = tbl->db->id;
    block->table_id = tbl->id;

    iterator_t* iter = list_iterator_create(tbl->column_new);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column iterator");

        memory_free(block);

        return false;
    }

    block->column_count = tbl->column_new_count;

    uint64_t col_idx = 0;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_column_t* col = (tosdb_column_t*)iter->delete_item(iter);

        block->columns[col_idx].id = col->id;
        strcopy(col->name, block->columns[col_idx].name);
        block->columns[col_idx].deleted = col->is_deleted;
        block->columns[col_idx].type = col->type;

        iter = iter->next(iter);

        col_idx++;
    }

    iter->destroy(iter);

    uint64_t loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

    if(loc == 0) {
        memory_free(block);

        return false;
    }

    tbl->column_list_location = loc;
    tbl->column_list_size = block->header.block_size;

    memory_free(block);


    tbl->column_new_count = 0;
    list_destroy(tbl->column_new);

    return true;
}

boolean_t tosdb_table_persist(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_FATAL, "table or db is null");

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "persisting table %s open? %i dirty? %i", tbl->name, tbl->is_open, tbl->is_dirty);

    if(!tbl->is_dirty) {
        return true;
    }

    if(!tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed");

        return false;
    }

    boolean_t need_persist = false;


    if(tbl->column_new_count) {
        need_persist = true;

        if(!tosdb_table_column_persist(tbl)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist column list for table %s", tbl->name);

            return false;
        }
    }

    if(tbl->index_new_count) {
        need_persist = true;

        if(!tosdb_table_index_persist(tbl)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist index list for table %s", tbl->name);

            return false;
        }
    }

    if(tbl->memtables && list_size(tbl->memtables)) {
        need_persist = true;
        PRINTLOG(TOSDB, LOG_TRACE, "persisting memtables for table %s memtable count %lli", tbl->name, list_size(tbl->memtables));

        if(!tosdb_table_memtable_persist(tbl)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist memtables for table %s", tbl->name);

            return false;
        }
    }

    if(!tbl->metadata_location) {
        need_persist = true;
    }

    if(need_persist) {
        tosdb_block_table_t* block = memory_malloc(TOSDB_PAGE_SIZE);

        if(!block) {
            return false;
        }

        block->header.block_size = TOSDB_PAGE_SIZE;
        block->header.block_type = TOSDB_BLOCK_TYPE_TABLE;
        block->header.previous_block_invalid = true;
        block->header.previous_block_location = tbl->metadata_location;
        block->header.previous_block_size = tbl->metadata_size;

        block->id = tbl->id;
        block->database_id = tbl->db->id;
        strcopy(tbl->name, block->name);
        block->column_next_id = tbl->column_next_id;
        block->index_next_id = tbl->index_next_id;
        block->column_list_location = tbl->column_list_location;
        block->column_list_size = tbl->column_list_size;
        block->index_list_location = tbl->index_list_location;
        block->index_list_size = tbl->index_list_size;
        block->memtable_next_id = tbl->memtable_next_id;
        block->sstable_list_location = tbl->sstable_list_location;
        block->sstable_list_size = tbl->sstable_list_size;
        block->primary_column_id = tbl->primary_column_id;
        block->primary_index_id = tbl->primary_index_id;
        block->primary_column_type = tbl->primary_column_type;

        // if compaction index id hint is not set, use primary index id
        if(tbl->compaction_index_id_hint != -1ULL) {
            block->compaction_index_id_hint = tbl->compaction_index_id_hint;
        } else {
            block->compaction_index_id_hint = tbl->primary_index_id;
        }

        uint64_t loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        tbl->metadata_location = loc;
        tbl->metadata_size = block->header.block_size;

        if(!tbl->db->table_new) {
            tbl->db->table_new = hashmap_integer(128);

            if(!tbl->db->table_new) {
                memory_free(block);

                return false;
            }
        }

        hashmap_put(tbl->db->table_new, (void*)tbl->id, tbl);

        PRINTLOG(TOSDB, LOG_DEBUG, "table %s is persisted at loc 0x%llx size 0x%llx", tbl->name, loc, block->header.block_size);

        tbl->is_dirty = false;
        tbl->db->is_dirty = true;

        memory_free(block);
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_table_column_add(tosdb_table_t* tbl, const char_t* colname, data_type_t type) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(strlen(colname) == 0 || strlen(colname) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "col name size error");

        return false;
    }

    if(hashmap_exists(tbl->columns, colname)) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s is exists for table %s", colname, tbl->name);

        return false;
    }

    if(!tbl->column_new) {
        tbl->column_new = list_create_list();

        if(!tbl->column_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new column list for table  %s", tbl->name);

            return false;
        }
    }

    tosdb_column_t* col = memory_malloc(sizeof(tosdb_column_t));

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column %s for table %s", colname, tbl->name);

        return false;
    }

    col->id = tbl->column_next_id;
    tbl->column_next_id++;

    col->name = strdup(colname);
    col->type = type;

    tbl->column_new_count++;

    hashmap_put(tbl->columns, col->name, col);
    list_list_insert(tbl->column_new, col);

    PRINTLOG(TOSDB, LOG_DEBUG, "col %s is added to table %s", colname, tbl->name);

    return true;
}

boolean_t tosdb_table_index_create(tosdb_table_t* tbl, const char_t* colname, tosdb_index_type_t type) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(strlen(colname) == 0 || strlen(colname) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "col name size error");

        return false;
    }

    const tosdb_column_t* col = hashmap_get(tbl->columns, colname);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s is not at table %s", colname, tbl->name);

        return false;
    }


    if(!tbl->index_new) {
        tbl->index_new = list_create_list();

        if(!tbl->index_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new index list for table  %s", tbl->name);

            return false;
        }
    }

    tosdb_index_t* idx = memory_malloc(sizeof(tosdb_index_t));

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index for column %s for table %s", colname, tbl->name);

        return false;
    }

    idx->id = tbl->index_next_id;
    tbl->index_next_id++;

    idx->column_id = col->id;
    idx->type = type;

    if(type == TOSDB_INDEX_PRIMARY) {
        tbl->primary_column_id = col->id;
        tbl->primary_index_id = idx->id;
        tbl->primary_column_type = col->type;
    }

    tbl->index_new_count++;

    hashmap_put(tbl->indexes, (void*)idx->id, idx);
    hashmap_put(tbl->index_column_map, (void*)idx->column_id, idx);
    list_list_insert(tbl->index_new, idx);

    PRINTLOG(TOSDB, LOG_DEBUG, "index %lli for column %s is added to table %s", idx->id, colname, tbl->name);

    return true;
}
#pragma GCC diagnostic pop

boolean_t tosdb_table_memtable_persist(tosdb_table_t* tbl) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(!tbl->memtables) {
        return true;
    }

    boolean_t error = false;

    lock_acquire(tbl->lock);

    uint64_t idx = list_size(tbl->memtables);

    do {
        idx--;

        tosdb_memtable_t* mt = (tosdb_memtable_t*)list_get_data_at_position(tbl->memtables, idx);

        if(!mt->is_dirty) {
            if(mt->stli) {
                list_stack_push(tbl->sstable_list_items, mt->stli);
                mt->stli = NULL;
            }

            continue;
        }

        if(!tosdb_memtable_persist(mt)) {
            error = true;
        }

        if(mt->stli) {
            list_stack_push(tbl->sstable_list_items, mt->stli);
            mt->stli = NULL;
        }

    } while(idx > 0);

    if(!list_size(tbl->sstable_list_items)) {
        return true;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "sstable list items count %lli", list_size(tbl->sstable_list_items));

    uint64_t block_size = sizeof(tosdb_block_sstable_list_t) + list_size(tbl->sstable_list_items) * sizeof(tosdb_block_sstable_list_item_t);


    buffer_t* buf_stli = buffer_new_with_capacity(NULL, block_size * 2);

    if(!buf_stli) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list buffer");
        lock_release(tbl->lock);

        return false;
    }

    iterator_t* iter = list_iterator_create(tbl->sstable_list_items);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable iter");
        lock_release(tbl->lock);

        return false;
    }

    uint64_t stli_cnt = list_size(tbl->sstable_list_items);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_block_sstable_list_item_t* stli = (tosdb_block_sstable_list_item_t*)iter->delete_item(iter);

        uint64_t size = sizeof(tosdb_block_sstable_list_item_t) + sizeof(tosdb_block_sstable_list_item_index_pair_t) * stli->index_count;

        buffer_append_bytes(buf_stli, (uint8_t*)stli, size);

        memory_free(stli);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    block_size = buffer_get_length(buf_stli);

    if(block_size % TOSDB_PAGE_SIZE) {
        block_size += TOSDB_PAGE_SIZE - (block_size % TOSDB_PAGE_SIZE);
    }

    tosdb_block_sstable_list_t* block = memory_malloc(block_size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create sstable list block");

        return false;
    }

    block->header.block_size = block_size;
    block->header.block_type = TOSDB_BLOCK_TYPE_SSTABLE_LIST;
    block->header.previous_block_location = tbl->sstable_list_location;
    block->header.previous_block_size = tbl->sstable_list_size;
    block->database_id = tbl->db->id;
    block->table_id = tbl->id;
    block->sstable_count = stli_cnt;

    uint8_t* ssts_loc = (uint8_t*)&block->sstables[0];

    buffer_write_all_into(buf_stli, ssts_loc);

    buffer_destroy(buf_stli);

    uint64_t block_loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

    memory_free(block);

    if(!block_loc) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write sstable list");

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "sstable list for table %s persisted at 0x%llx(0x%llx)", tbl->name, block_loc, block_size);

    tbl->sstable_list_size = block_size;
    tbl->sstable_list_location = block_loc;
    tbl->is_dirty = true;

    tbl->current_memtable = NULL;

    lock_release(tbl->lock);

    return !error;
}

set_t* tosdb_table_get_primary_keys(tosdb_table_t* tbl) {
    if(!tbl) {
        return NULL;
    }

    set_t* res = set_create(tosdb_record_primary_key_comparator);

    if(!res) {
        return NULL;
    }

    if(!tosdb_table_get_primary_keys_internal(tbl, res, NULL)) {
        set_destroy_with_callback(res, tosdb_record_search_set_destroy_cb);

        return NULL;
    }

    return res;
}

boolean_t tosdb_table_set_compaction_index_id_hint(tosdb_table_t* tbl, uint64_t index_id) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(!tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed");

        return false;
    }

    if(!hashmap_exists(tbl->indexes, (void*)index_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "index %lli is not exists for table %s", index_id, tbl->name);

        return false;
    }

    if(tbl->compaction_index_id_hint == index_id) {
        PRINTLOG(TOSDB, LOG_DEBUG, "index %lli is already set as compaction index id hint for table %s", index_id, tbl->name);

        return true;
    }

    tbl->compaction_index_id_hint = index_id;

    tbl->is_dirty = true;

    PRINTLOG(TOSDB, LOG_DEBUG, "index %lli is set as compaction index id hint for table %s", index_id, tbl->name);

    return true;
}

const tosdb_index_t*  tosdb_table_get_index_by_column_id(tosdb_table_t* tbl, uint64_t id) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return NULL;
    }

    if(!tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed");

        return NULL;
    }

    const tosdb_index_t* idx = hashmap_get(tbl->index_column_map, (void*)id);

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "index for column %lli not found", id);

        return NULL;
    }

    return idx;
}

boolean_t tosdb_table_set_compaction_index_id_hint_by_column_name(tosdb_table_t* tbl, const char_t* colname) {
    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is null");

        return false;
    }

    if(!tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed");

        return false;
    }

    if(strlen(colname) == 0 || strlen(colname) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "col name size error");

        return false;
    }

    const tosdb_column_t* col = hashmap_get(tbl->columns, colname);

    if(!col) {
        PRINTLOG(TOSDB, LOG_ERROR, "column %s is not at table %s", colname, tbl->name);

        return false;
    }

    const tosdb_index_t* idx = hashmap_get(tbl->index_column_map, (void*)col->id);

    if(!idx) {
        PRINTLOG(TOSDB, LOG_ERROR, "index for column %s not found", colname);

        return false;
    }

    if(tbl->compaction_index_id_hint == idx->id) {
        PRINTLOG(TOSDB, LOG_DEBUG, "index %lli is already set as compaction index id hint for table %s", idx->id, tbl->name);

        return true;
    }

    return tosdb_table_set_compaction_index_id_hint(tbl, idx->id);
}
