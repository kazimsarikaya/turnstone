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
#include <buffer.h>
#include <linkedlist.h>
#include <disk.h>
#include <set.h>

/*! tosdb backend struct type */
typedef struct tosdb_backend_t tosdb_backend_t;

/**
 * @brief creates new tosdb memory backend
 * @param[in] capacity memory backend max capacity
 * @return memory backend
 */
tosdb_backend_t* tosdb_backend_memory_new(uint64_t capacity);

/**
 * @brief creates new tosdb memory backend from buffer
 * @param[in] buffer memory backend buffer
 * @return memory backend
 */
tosdb_backend_t* tosdb_backend_memory_from_buffer(buffer_t buffer);

/**
 * @brief get memory backend contents
 * @param[in] backend memory backend
 * @return byte array of conntent
 */
uint8_t* tosdb_backend_memory_get_contents(tosdb_backend_t* backend);

/**
 * @brief creates new tosdb disk backend
 * @param[in] dp disk or partition
 * @return disk backend
 */
tosdb_backend_t* tosdb_backend_disk_new(disk_or_partition_t* dp);

/**
 * @brief closes and frees a backend
 * @param[in] backend the backend to operate
 * @return true if succeed.
 */
boolean_t tosdb_backend_close(tosdb_backend_t* backend);

/*! tosdb struct type */
typedef struct tosdb_t tosdb_t;

/**
 * @brief creates tosdb
 * @param[in] backend storage backend
 * @return new tosdb
 */
tosdb_t* tosdb_new(tosdb_backend_t* backend);

/**
 * @brief closes a tosdb
 * @param[in] tdb tosdb
 * @return true if succeed.
 */
boolean_t tosdb_close(tosdb_t* tdb);

/**
 * @brief frees a tosdb
 * @param[in] tdb tosdb
 * @return true if succeed.
 */
boolean_t tosdb_free(tosdb_t* tdb);

/**
 * @struct tosdb_cache_config_t
 * @brief tosdb cache config
 */
typedef struct tosdb_cache_config_t {
    uint64_t bloomfilter_size; ///< bloom filter cache max size
    uint64_t index_data_size; ///< index data cache max size
    uint64_t secondary_index_data_size; ///< index data cache max size
    uint64_t valuelog_size; ///< value log cache max size
} tosdb_cache_config_t; ///< shorthand for struct

/**
 * @brief sets tosdb cache config
 * @param[in] tdb tosdb instance
 * @param[in] config tosdb cache config
 * @return true if cache config can be setted
 */
boolean_t tosdb_cache_config_set(tosdb_t* tdb, tosdb_cache_config_t* config);

/**
 * @enum tosdb_compaction_type_t
 * @brief tosdb compation types.
 */
typedef enum tosdb_compaction_type_t {
    TOSDB_COMPACTION_TYPE_NONE, ///< tosdb compation type none
    TOSDB_COMPACTION_TYPE_MINOR, ///< tosdb compation type minor, compacts same level, removes duplicates, deleted ones
    TOSDB_COMPACTION_TYPE_MAJOR, ///< tosdb comaption type major, compacts whole level into a a high level
} tosdb_compaction_type_t;

boolean_t tosdb_compact(tosdb_t* tdb, tosdb_compaction_type_t type);

/*! tosdb database struct type */
typedef struct tosdb_database_t tosdb_database_t;

/**
 * @brief creates new database
 * @param[in] tdb tosdb interface
 * @param[in] name database name
 * @return a new database or existing one
 */
tosdb_database_t* tosdb_database_create_or_open(tosdb_t* tdb, const char_t* name);

/**
 * @brief closes a database
 * @param[in] db the database to close
 * @return true if succeed.
 *
 */
boolean_t tosdb_database_close(tosdb_database_t* db);

/**
 * @brief frees a database
 * @param[in] db the database to free
 * @return true if succeed.
 *
 */
boolean_t tosdb_database_free(tosdb_database_t* db);

/*! tosdb table struct type */
typedef struct tosdb_table_t tosdb_table_t;

/**
 * @brief creates new table
 * @param[in] db database interface
 * @param[in] name table name
 * @param[in] max_record_count maximum record count at each memtable/sstable
 * @param[in] max_valuelog_size maximum valuelog size
 * @param[in] max_memtable_count maximum memtable count at memory
 * @return a new table or existing one
 */
tosdb_table_t* tosdb_table_create_or_open(tosdb_database_t* db, const char_t* name, uint64_t max_record_count, uint64_t max_valuelog_size, uint64_t max_memtable_count);

/**
 * @brief adds a cloumn to given table
 * @param[in] tbl table interface
 * @param[in] colname cloumn name
 * @param[in] column type
 * @return true if succeed.
 */
boolean_t tosdb_table_column_add(tosdb_table_t* tbl, const char_t* colname, data_type_t type);

/**
 * @enum tosdb_index_type_t
 * @brief tosdb index types
 */
typedef enum tosdb_index_type_t {
    TOSDB_INDEX_PRIMARY, ///< primary index
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
boolean_t tosdb_table_index_create(tosdb_table_t* tbl, const char_t* colname, tosdb_index_type_t type);

/**
 * @brief closes a table
 * @param[in] tbl the table to close
 * @return true if succeed.
 *
 */
boolean_t tosdb_table_close(tosdb_table_t* tbl);

/**
 * @brief frees a table
 * @param[in] tbl the table to close
 * @return true if succeed.
 *
 */
boolean_t tosdb_table_free(tosdb_table_t* tbl);

/*! tosdb record struct type */
typedef struct tosdb_record_t tosdb_record_t;

/**
 * @brief set column value as boolean to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_boolean_f)(tosdb_record_t * record, const char_t* colname, const boolean_t value);

/**
 * @brief get column value as boolean to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_boolean_f)(tosdb_record_t * record, const char_t* colname, boolean_t* value);

/**
 * @brief set column value as char to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_char_f)(tosdb_record_t * record, const char_t* colname, const char_t value);

/**
 * @brief get column value as char to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_char_f)(tosdb_record_t * record, const char_t* colname, char_t* value);

/**
 * @brief set column value as int8 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_int8_f)(tosdb_record_t * record, const char_t* colname, const int8_t value);

/**
 * @brief get column value as int8 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_int8_f)(tosdb_record_t * record, const char_t* colname, int8_t* value);

/**
 * @brief set column value as uint8 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_uint8_f)(tosdb_record_t * record, const char_t* colname, const uint8_t value);

/**
 * @brief get column value as uint8 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_uint8_f)(tosdb_record_t * record, const char_t* colname, uint8_t* value);

/**
 * @brief set column value as int16 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_int16_f)(tosdb_record_t * record, const char_t* colname, const int16_t value);

/**
 * @brief get column value as int16 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_int16_f)(tosdb_record_t * record, const char_t* colname, int16_t* value);

/**
 * @brief set column value as uint16 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_uint16_f)(tosdb_record_t * record, const char_t* colname, const uint16_t value);

/**
 * @brief get column value as uint16 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_uint16_f)(tosdb_record_t * record, const char_t* colname, uint16_t* value);

/**
 * @brief set column value as int32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_int32_f)(tosdb_record_t * record, const char_t* colname, const int32_t value);

/**
 * @brief get column value as int32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_int32_f)(tosdb_record_t * record, const char_t* colname, int32_t* value);

/**
 * @brief set column value as uint32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_uint32_f)(tosdb_record_t * record, const char_t* colname, const uint32_t value);

/**
 * @brief get column value as uint32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_uint32_f)(tosdb_record_t * record, const char_t* colname, uint32_t* value);

/**
 * @brief set column value as int64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_int64_f)(tosdb_record_t * record, const char_t* colname, const int64_t value);

/**
 * @brief get column value as int64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_int64_f)(tosdb_record_t * record, const char_t* colname, int64_t* value);

/**
 * @brief set column value as uint64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_uint64_f)(tosdb_record_t * record, const char_t* colname, const uint64_t value);

/**
 * @brief get column value as uint64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_uint64_f)(tosdb_record_t * record, const char_t* colname, uint64_t* value);

/**
 * @brief set column value as string to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_string_f)(tosdb_record_t * record, const char_t* colname, const char_t* value);

/**
 * @brief get column value as string to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_string_f)(tosdb_record_t * record, const char_t* colname, char_t** value);

/**
 * @brief set column value as float32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_float32_f)(tosdb_record_t * record, const char_t* colname, const float32_t value);

/**
 * @brief get column value as float32 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_float32_f)(tosdb_record_t * record, const char_t* colname, float32_t* value);

/**
 * @brief set column value as float64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_float64_f)(tosdb_record_t * record, const char_t* colname, const float64_t value);

/**
 * @brief get column value as float64 to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_float64_f)(tosdb_record_t * record, const char_t* colname, float64_t* value);

/**
 * @brief set column value as byte array to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_bytearray_f)(tosdb_record_t * record, const char_t* colname, uint64_t len, const uint8_t* value);

/**
 * @brief get column value as byte array to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_bytearray_f)(tosdb_record_t * record, const char_t* colname, uint64_t* len, uint8_t** value);

/**
 * @brief set column value with data type to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] type data type
 * @param[in] value column value
 * @return true if value can be setted
 */
typedef boolean_t (*tosdb_record_set_data_f)(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t len, const void* value);

/**
 * @brief get column value as byte array to the record
 * @param[in] record record's itself
 * @param[in] colname column name
 * @param[in] type data type
 * @param[out] value column value
 * @return true if value can be getted
 */
typedef boolean_t (*tosdb_record_get_data_f)(tosdb_record_t * record, const char_t* colname, data_type_t type, uint64_t* len, void** value);


/**
 * @brief destroys (frees) record
 * @param[in] record record's itself
 * @return an iterator whose elements are tosdb_record_t
 */
typedef boolean_t (*tosdb_record_destroy_f)(tosdb_record_t* record);

/**
 * @brief updates or deletes record
 * @param[in] tbl table interface
 * @param[in] record record to upsert
 * @return true if succeed
 */
typedef boolean_t (*tosdb_record_upsert_f)(tosdb_record_t* record);

/**
 * @brief deletes a record
 * @param[in] tbl table interface
 * @param[in] record a unique key
 * @return deletes a record
 */
typedef boolean_t (*tosdb_record_delete_f)(tosdb_record_t* record);

/**
 * @brief gets a record
 * @param[in] tbl table interface
 * @param[in,out] record record for retrive. for in it must contain a indexed key. for out it is populated with values.
 * @return true if record found. record populated with values.
 */
typedef boolean_t (*tosdb_record_get_f)(tosdb_record_t* record);

/**
 * @brief searches a record
 * @param[in] tbl table interface
 * @param[in] record secondary key of record for retrive
 * @return the record list
 */
typedef linkedlist_t (*tosdb_record_search_f)(tosdb_record_t* record);

/**
 * @struct tosdb_record_t
 * @brief tosdb record
 */
struct tosdb_record_t {
    void*                        context;              ///< record context
    tosdb_record_set_boolean_f   set_boolean;      ///< set boolean
    tosdb_record_get_boolean_f   get_boolean;      ///< get boolean
    tosdb_record_set_char_f      set_char;      ///< set char
    tosdb_record_get_char_f      get_char;      ///< get char
    tosdb_record_set_int8_f      set_int8;      ///< set int8
    tosdb_record_get_int8_f      get_int8;      ///< get int8
    tosdb_record_set_uint8_f     set_uint8;      ///< set uint8
    tosdb_record_get_uint8_f     get_uint8;      ///< get uint8
    tosdb_record_set_int16_f     set_int16;      ///< set int16
    tosdb_record_get_int16_f     get_int16;      ///< get int16
    tosdb_record_set_uint16_f    set_uint16;      ///< set uint16
    tosdb_record_get_uint16_f    get_uint16;      ///< get uint16
    tosdb_record_set_int32_f     set_int32;      ///< set int32
    tosdb_record_get_int32_f     get_int32;      ///< get int32
    tosdb_record_set_uint32_f    set_uint32;      ///< set uint32
    tosdb_record_get_uint32_f    get_uint32;      ///< get uint32
    tosdb_record_set_int64_f     set_int64;      ///< set int64
    tosdb_record_get_int64_f     get_int64;      ///< get int64
    tosdb_record_set_uint64_f    set_uint64;      ///< set uint64
    tosdb_record_get_uint64_f    get_uint64;      ///< get uint64
    tosdb_record_set_string_f    set_string;      ///< set string
    tosdb_record_get_string_f    get_string;      ///< get string
    tosdb_record_set_float32_f   set_float32;      ///< set float32
    tosdb_record_get_float32_f   get_float32;      ///< get float32
    tosdb_record_set_float64_f   set_float64;      ///< set float64
    tosdb_record_get_float64_f   get_float64;      ///< get float64
    tosdb_record_set_bytearray_f set_bytearray;      ///< set bytearray
    tosdb_record_get_bytearray_f get_bytearray;      ///< set bytearray
    tosdb_record_set_data_f      set_data; ///< set data
    tosdb_record_get_data_f      get_data; ///< get data
    tosdb_record_get_f           get_record; ///< gets record from table
    tosdb_record_search_f        search_record; ///< search records with secondary index
    tosdb_record_upsert_f        upsert_record; ///< upsert record to the table
    tosdb_record_delete_f        delete_record; ///< delete record from table
    tosdb_record_destroy_f       destroy; ///< destroy record
};

/**
 * @brief creates a record for given table
 * @param[in] tbl table
 * @return record
 */
tosdb_record_t* tosdb_table_create_record(tosdb_table_t* tbl);

/**
 * @brief get all primary keys in terms of record
 * @param[in] tbl table
 * @return set of record with only contains primary key
 */
set_t* tosdb_table_get_primary_keys(tosdb_table_t* tbl);

 #endif

