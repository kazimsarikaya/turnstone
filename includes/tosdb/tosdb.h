/**
 * @file tosdb.h
 * @brief tosdb interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOSDB_TOSDB_H
#define ___TOSDB_TOSDB_H 0

#include <types.h>
#include <data.h>

///! tosdb backend struct type
typedef struct tosdb_backend_t tosdb_backend_t;

/**
 * @brief creates new tosdb memory backend
 * @param[in] capacity memory backend max capacity
 * @return memory backend
 */
tosdb_backend_t* tosdb_backend_memory_new(uint64_t capacity);

/**
 * @brief get memory backend contents
 * @param[in] backend memory backend
 * @return byte array of conntent
 */
uint8_t* tosdb_backend_memory_get_contents(tosdb_backend_t* backend);

/**
 * @brief closes and frees a backend
 * @param[in] backend the backend to operate
 * @return true if succeed.
 */
boolean_t tosdb_backend_close(tosdb_backend_t* backend);

///! tosdb struct type
typedef struct tosdb_t tosdb_t;

/**
 * @brief creates tosdb
 * @param[in] backend storage backend
 * @return new tosdb
 */
tosdb_t* tosdb_new(tosdb_backend_t* backend);

/**
 * @brief closes and destroys a tosdb
 * @param[in] tdb tosdb
 * @return true if succeed.
 */
boolean_t tosdb_close(tosdb_t* tdb);

///! tosdb database struct type
typedef struct tosdb_database_t tosdb_database_t;

/**
 * @brief creates new database
 * @param[in] tdb tosdb interface
 * @param[in] name database name
 * @return a new database or existing one
 */
tosdb_database_t* tosdb_database_create_or_open(tosdb_t* tdb, char_t* name);

/**
 * @brief closes and frees a database
 * @param[in] db the database to close
 * @return true if succeed.
 *
 */
boolean_t tosdb_database_close(tosdb_database_t* db);

///! tosdb table struct type
typedef struct tosdb_table_t tosdb_table_t;

/**
 * @brief creates new table
 * @param[in] db database interface
 * @param[in] name table name
 * @param[in] max_record_count maximum record count at each memtable/sstable
 * @param[in] max_valuelog_size maximum valuelog size
 * @return a new table or existing one
 */
tosdb_table_t* tosdb_table_create_or_open(tosdb_database_t* db, char_t* name, uint64_t max_record_count, uint64_t max_valuelog_size);

/**
 * @brief adds a cloumn to given table
 * @param[in] tbl table interface
 * @param[in] colname cloumn name
 * @param[in] column type
 * @return true if succeed.
 */
boolean_t tosdb_table_column_add(tosdb_table_t* tbl, char_t* colname, data_type_t type);

typedef enum tosdb_index_type_t {
    TOSDB_INDEX_UNIQUE, ///< a unique index
    TOSDB_INDEX_SECONDARY, ///< a secondary index
} tosdb_index_type_t; ///< shorthand for enum

/**
 * @brief creates an index on table
 * @param[in] tbl table interface
 * @param[in] colname index column name
 * @param[in] type index type
 * @return true if succeed.
 */
boolean_t tosdb_table_index_create(tosdb_table_t* tbl, char_t* colname, tosdb_index_type_t type);

/**
 * @brief closes and frees a table
 * @param[in] tbl the table to close
 * @return true if succeed.
 *
 */
boolean_t tosdb_table_close(tosdb_table_t* tbl);

/**
 * @brief updates or deletes record
 * @param[in] tbl table interface
 * @param[in] record record to upsert
 * @return true if succeed
 */
boolean_t tosdb_table_upsert(tosdb_table_t* tbl, data_t* record);

/**
 * @brief deletes a record
 * @param[in] tbl table interface
 * @param[in] key a unique key
 * @return deletes a record
 */
boolean_t tosdb_table_delete(tosdb_table_t* tbl, data_t* key);

/**
 * @brief gets a record
 * @param[in] tbl table interface
 * @param[in] key key for retrive
 * @return the record
 */
data_t* tosdb_table_get(tosdb_table_t* tbl, data_t* key);


#endif

