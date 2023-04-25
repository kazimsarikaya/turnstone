/**
 * @file tosdb_cache.h
 * @brief tosdb cache interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOSDB_TOSDB_CACHE_H
#define ___TOSDB_TOSDB_CACHE_H 0

#include <types.h>
#include <tosdb/tosdb.h>
#include <buffer.h>
#include <bloomfilter.h>


typedef enum tosdb_cache_item_type_t {
    TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER,
    TOSDB_CACHE_ITEM_TYPE_INDEX_DATA,
    TOSDB_CACHE_ITEM_TYPE_SECONDARY_INDEX_DATA,
    TOSDB_CACHE_ITEM_TYPE_VALUELOG,
} tosdb_cache_item_type_t;

typedef struct tosdb_memtable_index_item_t           tosdb_memtable_index_item_t;
typedef struct tosdb_memtable_secondary_index_item_t tosdb_memtable_secondary_index_item_t;

typedef struct tosdb_cache_key_t {
    tosdb_cache_item_type_t type;
    uint64_t                database_id;
    uint64_t                table_id;
    uint64_t                index_id;
    uint64_t                sstable_id;
    uint64_t                level;
    uint64_t                data_size;
} tosdb_cache_key_t;

typedef struct tosdb_cached_bloomfilter_t {
    tosdb_cache_key_t                      cache_key;
    tosdb_memtable_index_item_t*           first_key;
    tosdb_memtable_index_item_t*           last_key;
    tosdb_memtable_secondary_index_item_t* secondary_first_key;
    tosdb_memtable_secondary_index_item_t* secondary_last_key;
    bloomfilter_t*                         bloomfilter;
    uint64_t                               index_data_location;
    uint64_t                               index_data_size;
} tosdb_cached_bloomfilter_t;

typedef struct tosdb_cached_index_data_t {
    tosdb_cache_key_t             cache_key;
    uint64_t                      record_count;
    tosdb_memtable_index_item_t** index_items;
    uint64_t                      valuelog_location;
    uint64_t                      valuelog_size;
} tosdb_cached_index_data_t;

typedef struct tosdb_cached_secondary_index_data_t {
    tosdb_cache_key_t                       cache_key;
    uint64_t                                record_count;
    tosdb_memtable_secondary_index_item_t** index_items;
} tosdb_cached_secondary_index_data_t;

typedef struct tosdb_cached_valuelog_t {
    tosdb_cache_key_t cache_key;
    uint64_t          record_count;
    buffer_t          values;
} tosdb_cached_valuelog_t;

typedef struct tosdb_cache_t tosdb_cache_t;

tosdb_cache_t* tosdb_cache_new(tosdb_cache_config_t* config);
boolean_t      tosdb_cache_close(tosdb_cache_t* cache);

const tosdb_cache_key_t* tosdb_cache_get(tosdb_cache_t* cache, tosdb_cache_key_t* key);
boolean_t                tosdb_cache_put(tosdb_cache_t* cache, tosdb_cache_key_t* key);

#endif
