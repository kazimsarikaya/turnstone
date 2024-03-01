/**
 * @file tosdb.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_backend.h>
#include <tosdb/tosdb_cache.h>
#include <buffer.h>
#include <cpu/sync.h>
#include <logging.h>
#include <strings.h>
#include <xxhash.h>
#include <zpack.h>
#include <deflate.h>

MODULE("turnstone.kernel.db");

tosdb_t* tosdb_new(tosdb_backend_t* backend, compression_type_t compression_type_if_not_exists) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return NULL;
    }

    future_t* fut;

    fut = (future_t*)backend->read(backend, 0, sizeof(tosdb_superblock_t));

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read main super block");
    }

    tosdb_superblock_t* main_sb = future_get_data_and_destroy(fut);

    if(!main_sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read main superblock");

        return NULL;
    }

    boolean_t need_format = false;
    boolean_t need_repair = false;
    boolean_t main_superblock_failed = true;
    boolean_t backup_superblock_failed = true;

    if(strcmp(main_sb->header.signature, TOSDB_SUPERBLOCK_SIGNATURE) == 0) {
        uint64_t csum_bak = main_sb->header.checksum;
        main_sb->header.checksum = 0;
        uint64_t csum = xxhash64_hash(main_sb, main_sb->header.block_size);

        if(csum == csum_bak) {
            main_superblock_failed = false;
        } else {
            PRINTLOG(TOSDB, LOG_WARNING, "main block checksum mismatch 0x%llx 0x%llx", csum_bak, csum);
        }

        main_sb->header.checksum = csum_bak;
    } else {
        PRINTLOG(TOSDB, LOG_WARNING, "main block signature mismatch: %s", main_sb->header.signature);
    }

    if(main_superblock_failed) {
        PRINTLOG(TOSDB, LOG_WARNING, "main super block failed");
    }


    fut = (future_t*)backend->read(backend, backend->capacity - sizeof(tosdb_superblock_t), sizeof(tosdb_superblock_t));

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read backup super block");

        memory_free_ext(backend->heap, main_sb);

        return NULL;
    }

    tosdb_superblock_t* backup_sb = future_get_data_and_destroy(fut);

    if(!backup_sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read backup superblock");

        memory_free(main_sb);

        return NULL;
    }

    if(strcmp(backup_sb->header.signature, TOSDB_SUPERBLOCK_SIGNATURE) == 0) {
        uint64_t csum_bak = backup_sb->header.checksum;
        backup_sb->header.checksum = 0;
        uint64_t csum = xxhash64_hash(backup_sb, backup_sb->header.block_size);

        if(csum == csum_bak) {
            backup_superblock_failed = false;
        } else {
            PRINTLOG(TOSDB, LOG_WARNING, "backup block checksum mismatch 0x%llx 0x%llx", csum_bak, csum);
        }

        backup_sb->header.checksum = csum_bak;
    } else {
        PRINTLOG(TOSDB, LOG_WARNING, "backup block signature mismatch: %s", backup_sb->header.signature);
    }


    if(backup_superblock_failed) {
        PRINTLOG(TOSDB, LOG_WARNING, "backup super block invalid");
    }

    if(main_superblock_failed && !backup_superblock_failed) {
        need_repair = true;
        memory_free(main_sb);
    }

    if(main_superblock_failed && backup_superblock_failed) {
        need_format = true;
        memory_free(main_sb);
    }

    memory_free(backup_sb);

    if(need_repair) {
        PRINTLOG(TOSDB, LOG_WARNING, "backend needs reparing");

        main_sb = tosdb_backend_repair(backend);

        if(!main_sb) {
            return NULL;
        }
    }

    if(need_format) {
        PRINTLOG(TOSDB, LOG_WARNING, "backend needs format");

        main_sb = tosdb_backend_format(backend, compression_type_if_not_exists);

        if(!main_sb) {
            return NULL;
        }
    }

    tosdb_t* res = memory_malloc(sizeof(tosdb_t));

    if(!res) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create tosdb struct");
        memory_free(main_sb);

        return NULL;
    }

    res->backend = backend;
    res->superblock = main_sb;

    if(main_sb->compression_type == COMPRESSION_TYPE_NONE) {
        res->compression = compression_get(compression_type_if_not_exists);

        if(!res->compression) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get compression type %d", compression_type_if_not_exists);
            memory_free(main_sb);
            memory_free(res);

            return NULL;
        }
    } else {
        res->compression = compression_get(main_sb->compression_type);

        if(!res->compression) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get compression type %d", compression_type_if_not_exists);
            memory_free(main_sb);
            memory_free(res);

            return NULL;
        }
    }

    if(!tosdb_load_databases(res)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load databases");
        memory_free(main_sb);
        memory_free(res);

        return NULL;
    }

    res->lock = lock_create();

    return res;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_load_databases(tosdb_t* tdb) {
    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return false;
    }

    if(!tdb->databases) {
        tdb->databases = hashmap_string(128);
    }

    if(!tdb->databases) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create database map");

        return false;
    }

    uint64_t db_list_loc = tdb->superblock->database_list_location;
    uint64_t db_list_size = tdb->superblock->database_list_size;

    while(db_list_loc != 0) {
        tosdb_block_database_list_t* db_list = (tosdb_block_database_list_t*)tosdb_block_read(tdb, db_list_loc, db_list_size);

        if(!db_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read database list");

            return false;
        }

        char_t name_buf[TOSDB_NAME_MAX_LEN + 1] = {0};

        for(uint64_t i = 0; i < db_list->database_count; i++) {
            memory_memclean(name_buf, TOSDB_NAME_MAX_LEN + 1);
            memory_memcopy(db_list->databases[i].name, name_buf, TOSDB_NAME_MAX_LEN);

            if(hashmap_exists(tdb->databases, name_buf)) {
                continue;
            }

            tosdb_database_t* db = memory_malloc(sizeof(tosdb_database_t));

            if(!db) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate db");
                memory_free(db_list);

                return false;
            }

            db->tdb = tdb;
            db->id = db_list->databases[i].id;
            db->name = strdup(name_buf);
            db->lock = lock_create();
            db->is_deleted = db_list->databases[i].deleted;
            db->metadata_location = db_list->databases[i].metadata_location;
            db->metadata_size = db_list->databases[i].metadata_size;

            hashmap_put(tdb->databases, db->name, db);

            PRINTLOG(TOSDB, LOG_DEBUG, "database %s lazy loaded. md loc 0x%llx(0x%llx)", db->name, db->metadata_location, db->metadata_size);
        }


        if(db_list->header.previous_block_invalid) {
            break;
        }

        db_list_loc = db_list->header.previous_block_location;
        db_list_size = db_list->header.previous_block_size;

        memory_free(db_list);
    }

    return true;
}
#pragma GCC diagnostic pop

boolean_t tosdb_close(tosdb_t* tdb) {
    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return false;
    }

    if(!tdb->backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb backend is null");

        return false;
    }

    if(!tdb->superblock) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb superblock is null");

        return false;
    }

    boolean_t error = false;

    iterator_t* iter = hashmap_iterator_create(tdb->databases);

    while (iter->end_of_iterator(iter) != 0) {
        tosdb_database_t * db = (tosdb_database_t *)iter->get_item(iter);

        if (!tosdb_database_close(db)) {
            PRINTLOG(TOSDB, LOG_ERROR, "database %s cannot be closed", db->name);
            error = true;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if (tdb->is_dirty) {
        if (!tosdb_persist(tdb)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist tosdb metadata");

            return false;
        }
    }

    return !error;
}

boolean_t tosdb_free(tosdb_t * tdb) {
    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return false;
    }

    if(!tdb->backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb backend is null");

        return false;
    }

    if(!tdb->superblock) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb superblock is null");

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "tosdb will be freed");

    boolean_t error = false;

    iterator_t* iter = hashmap_iterator_create(tdb->databases);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_database_t* db = (tosdb_database_t*)iter->get_item(iter);

        if(!tosdb_database_free(db)) {
            PRINTLOG(TOSDB, LOG_ERROR, "database %s cannot be freed", db->name);
            error = true;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    memory_free(tdb->superblock);
    lock_destroy(tdb->lock);
    hashmap_destroy(tdb->databases);
    hashmap_destroy(tdb->database_new);
    tosdb_cache_close(tdb->cache);
    memory_free(tdb);

    PRINTLOG(TOSDB, LOG_DEBUG, "tosdb freed");

    return !error;
}

tosdb_block_header_t* tosdb_block_read(tosdb_t* tdb, uint64_t location, uint64_t size) {
    if(!tdb || !location || !size || (size % TOSDB_PAGE_SIZE) ) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null or location/size (0x%llx,0x%llx) is zero or size isnot multiple of tosdb page size", location, size);

        return NULL;
    }

    tosdb_block_header_t* block = (tosdb_block_header_t*)tdb->backend->read(tdb->backend, location, size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read block");

        return NULL;
    }

    if(size != block->block_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read block");

        memory_free(block);

        return NULL;
    }

    if(strcmp(TOSDB_SUPERBLOCK_SIGNATURE, block->signature) != 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "block signature mismatch");

        memory_free(block);

        return NULL;
    }


    uint64_t csum_bak = block->checksum;
    block->checksum = 0;
    uint64_t csum = xxhash64_hash(block, block->block_size);
    block->checksum = csum_bak;

    if(csum != csum_bak) {
        PRINTLOG(TOSDB, LOG_ERROR, "checksum mismatch");

        memory_free(block);

        return NULL;

    }

    return block;
}

uint64_t tosdb_block_write(tosdb_t* tdb, tosdb_block_header_t* block) {
    if(!tdb || !block) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb or block is null");

        return 0;
    }

    strcpy(TOSDB_SUPERBLOCK_SIGNATURE, block->signature);
    block->version_major = TOSDB_VERSION_MAJOR;
    block->version_minor = TOSDB_VERSION_MINOR;

    uint64_t csum = xxhash64_hash(block, block->block_size);

    block->checksum = csum;

    uint64_t w_cnt = tdb->backend->write(tdb->backend, tdb->superblock->free_next_location, block->block_size, (uint8_t*)block);

    if(w_cnt != block->block_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write block");

        return false;
    }

    uint64_t res = tdb->superblock->free_next_location;

    tdb->superblock->free_next_location += block->block_size;

    return res;
}


boolean_t tosdb_persist(tosdb_t* tdb) {
    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb struct is null");

        return false;
    }

    boolean_t error = false;

    uint64_t metadata_size = sizeof(tosdb_block_database_list_t) + sizeof(tosdb_block_database_list_item_t) * hashmap_size(tdb->database_new);

    if(metadata_size % TOSDB_PAGE_SIZE) {
        metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));
    }

    tosdb_block_database_list_t * block = memory_malloc(metadata_size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create database list");

        return false;
    }

    block->header.block_type = TOSDB_BLOCK_TYPE_DATABASE_LIST;
    block->header.block_size = metadata_size;
    block->header.previous_block_location = tdb->superblock->database_list_location;
    block->header.previous_block_size = tdb->superblock->database_list_size;

    iterator_t* iter = hashmap_iterator_create(tdb->database_new);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create database iterator");

        memory_free(block);

        return false;
    }

    block->database_count = hashmap_size(tdb->database_new);

    uint64_t db_idx = 0;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_database_t* db = (tosdb_database_t*)iter->get_item(iter);

        if(db->is_dirty) {
            if(!tosdb_database_persist(db)) {
                error = true;

                break;
            }
        }

        block->databases[db_idx].id = db->id;
        strcpy(db->name, block->databases[db_idx].name);
        block->databases[db_idx].deleted = db->is_deleted;

        if(!db->is_deleted) {
            block->databases[db_idx].metadata_location = db->metadata_location;
            block->databases[db_idx].metadata_size = db->metadata_size;
        }

        iter = iter->next(iter);

        db_idx++;
    }

    iter->destroy(iter);


    if(error) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot persist one of dirty database");
        memory_free(block);

        return false;
    }

    uint64_t loc = tosdb_block_write(tdb, (tosdb_block_header_t*)block);

    if(loc == 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write database list");

        memory_free(block);

        return false;
    }

    tdb->superblock->database_list_location = loc;
    tdb->superblock->database_list_size = block->header.block_size;

    PRINTLOG(TOSDB, LOG_DEBUG, "database list loc 0x%llx size 0x%llx", loc, block->header.block_size);

    memory_free(block);

    hashmap_destroy(tdb->database_new);
    tdb->database_new = NULL;

    if(!tosdb_write_and_flush_superblock(tdb->backend, tdb->superblock)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write and flush super block");

        return false;
    }

    tdb->is_dirty = false;

    return true;
}

boolean_t tosdb_cache_config_set(tosdb_t* tdb, tosdb_cache_config_t* config) {
    if(!tdb || !config) {
        PRINTLOG(TOSDB, LOG_ERROR, "required fields are null");

        return false;
    }

    if(tdb->cache) {
        PRINTLOG(TOSDB, LOG_ERROR, "cache config already setted");

        return false;
    }

    tdb->cache = tosdb_cache_new(config);

    if(!tdb->cache) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create tosdb cache");

        return false;
    }

    return true;
}
