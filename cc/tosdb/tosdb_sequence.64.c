/**
 * @file tosdb_sequence.64.c
 * @brief TOSDB sequence functions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <logging.h>

MODULE("turnstone.kernel.db");

tosdb_sequence_t* tosdb_sequence_create_or_open(tosdb_database_t* db, const char_t* name, int64_t start, int64_t cache_size) {
    if(db == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "db is null");

        return NULL;
    }

    if(name == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "name is null");

        return NULL;
    }

    if(start < 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "start is less than zero");

        return NULL;
    }

    if(cache_size < 1) {
        PRINTLOG(TOSDB, LOG_ERROR, "cache_size is less than one");

        return NULL;
    }

    tosdb_sequence_t* seq = (tosdb_sequence_t*)hashmap_get(db->sequences, name);

    if(seq != NULL) {
        return seq;
    }

    tosdb_table_t* seq_table = tosdb_table_create_or_open(db, TOSDB_SEQUENCE_TABLE_NAME, 1 << 10, 10 << 10, 2);

    if (seq_table == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot open sequence table");

        return NULL;
    }

    if(hashmap_size(seq_table->columns) == 0) {
        if(!tosdb_table_column_add(seq_table, "id", DATA_TYPE_INT64)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot add id column to sequence table");

            return NULL;
        }

        if(!tosdb_table_column_add(seq_table, "name", DATA_TYPE_STRING)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot add name column to sequence table");

            return NULL;
        }

        if(!tosdb_table_column_add(seq_table, "next_value", DATA_TYPE_INT64)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot add next_value column to sequence table");

            return NULL;
        }

        if(!tosdb_table_index_create(seq_table, "id", TOSDB_INDEX_PRIMARY)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index for id column in sequence table");

            return NULL;
        }

        if(!tosdb_table_index_create(seq_table, "name", TOSDB_INDEX_UNIQUE)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index for name column in sequence table");

            return NULL;
        }

        tosdb_record_t* record = tosdb_table_create_record(seq_table);

        if(record == NULL) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create record for sequence table");

            return NULL;
        }

        if(!record->set_int64(record, "id", 1)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set id column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->set_string(record, "name", "default")) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set name column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->set_int64(record, "next_value", 2)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set next_value column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->upsert_record(record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record for sequence table");
            record->destroy(record);

            return NULL;
        }

        record->destroy(record);
    }

    tosdb_record_t* record = tosdb_table_create_record(seq_table);
    int64_t seq_id = 0;
    int64_t seq_next_value = 0;

    if(record == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record for sequence table");

        return NULL;
    }

    if(!record->set_string(record, "name", name)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set name column for sequence table");
        record->destroy(record);

        return NULL;
    }

    if(!record->get_record(record)) {
        tosdb_record_t* default_record = tosdb_table_create_record(seq_table);

        if(default_record == NULL) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create record for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!default_record->set_int64(default_record, "id", 1)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set id column for sequence table");
            record->destroy(record);
            default_record->destroy(default_record);

            return NULL;
        }

        if(!default_record->get_record(default_record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get record for sequence table");
            record->destroy(record);
            default_record->destroy(default_record);

            return NULL;
        }

        if(!default_record->get_int64(default_record, "next_value", &seq_id)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get next_value column for sequence table");
            record->destroy(record);
            default_record->destroy(default_record);

            return NULL;
        }

        if(!default_record->set_int64(default_record, "next_value", seq_id + 1)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set id column for sequence table");
            record->destroy(record);
            default_record->destroy(default_record);

            return NULL;
        }

        if(!default_record->upsert_record(default_record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record for sequence table");
            record->destroy(record);
            default_record->destroy(default_record);

            return NULL;
        }

        default_record->destroy(default_record);

        PRINTLOG(TOSDB, LOG_TRACE, "create sequence table record for %s with id %lli", name, seq_id);

        if(!record->set_int64(record, "id", seq_id)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set id column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->set_int64(record, "next_value", start + cache_size)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set next_value column for sequence table");
            record->destroy(record);

            return NULL;
        }

        seq_next_value = start;

        if(!record->upsert_record(record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record for sequence table");
            record->destroy(record);

            return NULL;
        }

    } else {
        if(!record->get_int64(record, "id", &seq_id)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get id column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->get_int64(record, "next_value", &seq_next_value)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get next_value column for sequence table");
            record->destroy(record);

            return NULL;
        }

        PRINTLOG(TOSDB, LOG_TRACE, "get sequence table record for %s (%lli) with next_value %lli", name, seq_id, seq_next_value);

        if(!record->set_int64(record, "next_value", seq_next_value + cache_size)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set next_value column for sequence table");
            record->destroy(record);

            return NULL;
        }

        if(!record->upsert_record(record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record for sequence table");
            record->destroy(record);

            return NULL;
        }
    }

    seq = memory_malloc(sizeof(tosdb_sequence_t));

    if(seq == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate memory for sequence");
        record->destroy(record);

        return NULL;
    }

    seq->this_record = record;
    seq->id = seq_id;
    seq->next_value = seq_next_value;
    seq->cache_size = cache_size;
    seq->cache_current_size = cache_size;
    seq->lock = lock_create();

    if(seq->lock == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create lock for sequence");
        record->destroy(record);
        memory_free(seq);

        return NULL;
    }

    if(db->sequences == NULL) {
        db->sequences = hashmap_string(128);

        if(db->sequences == NULL) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create hashmap for sequences");
            record->destroy(record);
            lock_destroy(seq->lock);
            memory_free(seq);

            return NULL;
        }
    }

    PRINTLOG(TOSDB, LOG_TRACE, "create/open sequence %s (%lli) next_value %lli", name, seq_id, seq_next_value);
    hashmap_put(db->sequences, name, seq);

    return seq;
}



int64_t tosdb_sequence_next(tosdb_sequence_t* seq) {
    if(seq == NULL) {
        PRINTLOG(TOSDB, LOG_ERROR, "sequence is null");

        return -1;
    }

    int64_t next_value = 0;

    lock_acquire(seq->lock);

    if(seq->cache_current_size == 0) {
        if(!seq->this_record->set_int64(seq->this_record, "next_value", seq->next_value + seq->cache_size)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot set next_value column for sequence table");
            lock_release(seq->lock);

            return -1;
        }

        if(!seq->this_record->upsert_record(seq->this_record)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record for sequence table");
            lock_release(seq->lock);

            return -1;
        }

        seq->cache_current_size = seq->cache_size;
    }

    next_value = seq->next_value++;
    seq->cache_current_size--;

    lock_release(seq->lock);


    return next_value;
}
