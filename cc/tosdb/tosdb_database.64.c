/**
 * @file tosdb_database.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <strings.h>

tosdb_database_t* tosdb_database_create_or_open(tosdb_t* tdb, char_t* name) {
    if(strlen(name) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "database name cannot be longer than %i", TOSDB_NAME_MAX_LEN);
        return NULL;
    }

    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return NULL;
    }


    if(map_exists(tdb->databases, name)) {
        return (tosdb_database_t*)map_get(tdb->databases, name);
    }

    if(!tdb->database_new) {
        tdb->database_new = linkedlist_create_list();

        if(!tdb->database_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new database list");

            return NULL;
        }
    }

    lock_acquire(tdb->lock);

    tosdb_database_t* db = memory_malloc(sizeof(tosdb_database_t));

    if(!db) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create db struct");

        lock_release(tdb->lock);

        return NULL;
    }


    db->id = tdb->superblock->database_next_id;

    tdb->superblock->database_next_id++;
    tdb->is_dirty = true;

    db->tdb = tdb;
    db->name = strdup(name);

    db->is_open = true;
    db->is_dirty = true;

    db->table_next_id = 1;
    db->tables = map_string();

    map_insert(tdb->databases, name, db);

    linkedlist_list_insert(tdb->database_new, db);

    tdb->database_new_count++;


    lock_release(tdb->lock);

    return db;
}

boolean_t tosdb_database_close(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");
    }


    iterator_t* iter = map_create_iterator(db->tables);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create table iterator");

        return false;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_table_t* tbl = (tosdb_table_t*)iter->get_item(iter);

        if(!tosdb_table_close(tbl)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot close table %s", tbl->name);

            iter->destroy(iter);

            return false;
        }

        iter = iter->next(iter);
    }


    iter->destroy(iter);


    memory_free(db->name);

    map_destroy(db->tables);

    memory_free(db);

    return true;
}

boolean_t tosdb_database_persist(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_FATAL, "db or tosdb is null");

        return false;
    }

    if(!db->is_dirty) {
        return true;
    }

    if(!db->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "database is closed");

        return false;
    }

    boolean_t need_persist = false;


    if(db->table_new_count) {
        need_persist = true;

        boolean_t error = false;

        uint64_t metadata_size = sizeof(tosdb_block_table_list_t) + sizeof(tosdb_block_table_list_item_t) * db->table_new_count;
        metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));

        tosdb_block_table_list_t* block = memory_malloc(metadata_size);

        if(!block) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create database list");

            return false;
        }

        block->header.block_type = TOSDB_BLOCK_TYPE_TABLE_LIST;
        block->header.block_size = metadata_size;
        block->header.previous_block_location = db->table_list_location;
        block->header.previous_block_size = db->table_list_size;

        iterator_t* iter = linkedlist_iterator_create(db->table_new);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create database iterator");

            memory_free(block);

            return false;
        }

        block->table_count = db->table_new_count;

        uint64_t tbl_idx = 0;

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_table_t* tbl = (tosdb_table_t*)iter->delete_item(iter);

            if(tbl->is_dirty) {
                if(!tosdb_table_persist(tbl)) {
                    error = true;

                    break;
                }
            }

            block->tables[tbl_idx].id = tbl->id;
            strcpy(tbl->name, block->tables[tbl_idx].name);
            block->tables[tbl_idx].deleted = tbl->is_deleted;

            if(!db->is_deleted) {
                block->tables[tbl_idx].metadata_location = tbl->metadata_location;
                block->tables[tbl_idx].metadata_size = tbl->metadata_size;
            }

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        if(error) {
            memory_free(block);

            return true;
        }

        uint64_t loc = tosdb_block_write(db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        db->table_list_location = loc;
        db->table_list_size = block->header.block_size;

        memory_free(block);


        db->table_new_count = 0;
        linkedlist_destroy(db->table_new);

    }


    if(!db->metadata_location) {
        need_persist = true;
    }

    if(need_persist) {
        tosdb_block_database_t* block = memory_malloc(TOSDB_PAGE_SIZE);

        if(!block) {
            return false;
        }

        block->header.block_size = TOSDB_PAGE_SIZE;
        block->header.block_type = TOSDB_BLOCK_TYPE_DATABASE;
        block->header.previous_block_invalid = true;
        block->header.previous_block_location = db->metadata_location;
        block->header.previous_block_size = db->metadata_size;

        block->id = db->id;
        strcpy(db->name, block->name);

        uint64_t loc = tosdb_block_write(db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        db->metadata_location = loc;
        db->metadata_size = block->header.block_size;

        PRINTLOG(TOSDB, LOG_DEBUG, "database loc 0x%llx size 0x%llx", loc, block->header.block_size);

        memory_free(block);
    }

    return true;
}
