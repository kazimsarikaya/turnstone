/**
 * @file tosdb_compaction.64.c
 * @brief tosdb compaction implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_compact(tosdb_t* tdb, tosdb_compaction_type_t type) {
    if(!tdb) {
        return false;
    }

    if(type == TOSDB_COMPACTION_TYPE_NONE) {
        return true;
    }

    if(type == TOSDB_COMPACTION_TYPE_MAJOR) {
        //if(!tosdb_compact(tdb, TOSDB_COMPACTION_TYPE_MINOR)) {
        //return false;
        //}
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

        error = !tosdb_table_compact(tbl, type);

        tbl_iter = tbl_iter->next(tbl_iter);
    }

    tbl_iter->destroy(tbl_iter);

    return !error;
}

boolean_t tosdb_table_compact(const tosdb_table_t* tbl, tosdb_compaction_type_t type) {
    if(!tbl) {
        return false;
    }

    if(type == TOSDB_COMPACTION_TYPE_NONE) {
        return true;
    }

    boolean_t error = false;

    set_t* pks = set_create(tosdb_record_primary_key_comparator);
    linkedlist_t old_pks = linkedlist_create_list();
    tosdb_table_get_primary_keys_internal(tbl, pks, old_pks);

    printf("!!! ss %s %lli\n", tbl->name, set_size(pks));
    set_destroy_with_callback(pks, tosdb_record_search_set_destroy_cb);

    printf("!!! ls %s %lli\n", tbl->name, linkedlist_size(old_pks));

    iterator_t* iter = linkedlist_iterator_create(old_pks);

    hashmap_t* level_holes = hashmap_integer(128);

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_t* rec = (tosdb_record_t*)iter->get_item(iter);
        tosdb_record_context_t* ctx = rec->context;

        if(hashmap_exists(level_holes, (void*)ctx->level)) {
            hashmap_t* st_holes = (hashmap_t*)hashmap_get(level_holes, (void*)ctx->level);
            hashmap_put(st_holes, (void*)ctx->sstable_id, (void*)((uint64_t)hashmap_get(st_holes, (void*)ctx->sstable_id) + 1));
        } else {
            hashmap_t* st_holes = hashmap_integer(128);
            hashmap_put(st_holes, (void*)ctx->sstable_id, (void*)1);
            hashmap_put(level_holes, (void*)ctx->level, st_holes);
        }

        rec->destroy(rec);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
    linkedlist_destroy(old_pks);

    iter = hashmap_iterator_create(level_holes);

    while(iter->end_of_iterator(iter) != 0) {
        hashmap_t* st_holes = (hashmap_t*)iter->get_item(iter);
        uint64_t level = (uint64_t)iter->get_extra_data(iter);

        iterator_t* st_iter = hashmap_iterator_create(st_holes);

        while(st_iter->end_of_iterator(st_iter) != 0) {
            uint64_t hole_count = (uint64_t)st_iter->get_item(st_iter);
            uint64_t st_id = (uint64_t)st_iter->get_extra_data(st_iter);

            printf("!!! %lli %lli %lli\n", level, st_id, hole_count);

            st_iter = st_iter->next(st_iter);
        }

        st_iter->destroy(st_iter);
        hashmap_destroy(st_holes);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
    hashmap_destroy(level_holes);

    for(uint64_t i = 1; i < tbl->sstable_max_level; i++) {
        if(type == TOSDB_COMPACTION_TYPE_MINOR) {
            error = !tosdb_sstable_level_minor_compact(tbl, i);
        } else {
            error = !tosdb_sstable_level_major_compact(tbl, i);
        }
    }

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
