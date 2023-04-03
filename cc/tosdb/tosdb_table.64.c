/**
 * @file tosdb_table.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <strings.h>

tosdb_table_t* tosdb_table_create_or_open(tosdb_database_t* db, char_t* name, uint64_t max_record_count, uint64_t max_valuelog_size) {
    UNUSED(db);
    UNUSED(name);
    UNUSED(max_record_count);
    UNUSED(max_valuelog_size);

    NOTIMPLEMENTEDLOG(TOSDB);

    return NULL;
}

boolean_t tosdb_table_close(tosdb_table_t* tbl) {
    UNUSED(tbl);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_table_column_add(tosdb_table_t* tbl, char_t* colname, data_type_t type) {
    UNUSED(tbl);
    UNUSED(colname);
    UNUSED(type);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

boolean_t tosdb_table_persist(tosdb_table_t* tbl) {
    UNUSED(tbl);

    NOTIMPLEMENTEDLOG(TOSDB);

    return false;
}

