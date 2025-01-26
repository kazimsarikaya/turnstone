/**
 * @file tosdb_cache.h
 * @brief tosdb cache interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOSDB_TOSDB_CACHE_H
/*! macro for preventing second time include */
#define ___TOSDB_TOSDB_CACHE_H 0

#include <types.h>
#include <tosdb/tosdb.h>
#include <buffer.h>
#include <bloomfilter.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum tosdb_cache_item_type_t
 * @brief tosdb cache item type
 */
typedef enum tosdb_cache_item_type_t {
    TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER, ///< bloomfilter
    TOSDB_CACHE_ITEM_TYPE_INDEX_DATA, ///< index data
    TOSDB_CACHE_ITEM_TYPE_SECONDARY_INDEX_DATA, ///< secondary index data
    TOSDB_CACHE_ITEM_TYPE_VALUELOG, ///< valuelog
} tosdb_cache_item_type_t; ///< tosdb cache item type

/**
 * @typedef tosdb_memtable_index_item_t
 * @brief opaque tosdb memtable index item
 */
typedef struct tosdb_memtable_index_item_t tosdb_memtable_index_item_t;

/**
 * @typedef tosdb_memtable_secondary_index_item_t
 * @brief opaque tosdb memtable secondary index item
 */
typedef struct tosdb_memtable_secondary_index_item_t tosdb_memtable_secondary_index_item_t;

/**
 * @struct tosdb_cache_key_t
 * @brief tosdb cache key
 */
typedef struct tosdb_cache_key_t {
    tosdb_cache_item_type_t type; ///< cache item type
    uint64_t                database_id; ///< database id
    uint64_t                table_id; ///< table id
    uint64_t                index_id; ///< index id
    uint64_t                sstable_id; ///< sstable id
    uint64_t                level; ///< level
    uint64_t                data_size; ///< data size
} tosdb_cache_key_t; ///< tosdb cache key

/**
 * @struct tosdb_cached_bloomfilter_t
 * @brief tosdb boomfilter cache item
 */
typedef struct tosdb_cached_bloomfilter_t {
    tosdb_cache_key_t                      cache_key; ///< cache key
    tosdb_memtable_index_item_t*           first_key; ///< first key in bloomfilter
    tosdb_memtable_index_item_t*           last_key; ///< last key in bloomfilter
    tosdb_memtable_secondary_index_item_t* secondary_first_key; ///< if secondary index exists, first key in bloomfilter
    tosdb_memtable_secondary_index_item_t* secondary_last_key; ///< if secondary index exists, last key in bloomfilter
    bloomfilter_t*                         bloomfilter; ///< bloomfilter
    uint64_t                               index_data_location; ///< index data location bloomfilter belongs to
    uint64_t                               index_data_size; ///< index data size bloomfilter belongs to
} tosdb_cached_bloomfilter_t; ///< tosdb boomfilter cache item

/**
 * @struct tosdb_cached_index_data_t
 * @brief tosdb index data cache item it is for primary/unique index
 */
typedef struct tosdb_cached_index_data_t {
    tosdb_cache_key_t             cache_key; ///< cache key
    uint64_t                      record_count; ///< record count at index data
    tosdb_memtable_index_item_t** index_items; ///< index items
    uint64_t                      valuelog_location; ///< valuelog location index data belongs to
    uint64_t                      valuelog_size; ///< valuelog size index data belongs to
} tosdb_cached_index_data_t; ///< tosdb index data cache item

/**
 * @struct tosdb_cached_secondary_index_data_t
 * @brief tosdb secondary index data cache item
 */
typedef struct tosdb_cached_secondary_index_data_t {
    tosdb_cache_key_t                       cache_key; ///< cache key
    uint64_t                                record_count; ///< record count at secondary index data
    tosdb_memtable_secondary_index_item_t** index_items; ///< index items
} tosdb_cached_secondary_index_data_t; ///< tosdb secondary index data cache item

/**
 * @struct tosdb_cached_valuelog_t
 * @brief tosdb valuelog cache item
 */
typedef struct tosdb_cached_valuelog_t {
    tosdb_cache_key_t cache_key; ///< cache key
    uint64_t          record_count; ///< record count at valuelog
    buffer_t*         values; ///< values
} tosdb_cached_valuelog_t; ///< tosdb valuelog cache item

/**
 * @typedef tosdb_cache_t
 * @brief opaque tosdb cache
 */
typedef struct tosdb_cache_t tosdb_cache_t;

/**
 * @brief creates new tosdb cache
 * @param config cache config
 * @return tosdb cache if success, NULL otherwise
 */
tosdb_cache_t* tosdb_cache_new(tosdb_cache_config_t* config);

/**
 * @brief deletes tosdb cache
 * @param cache tosdb cache
 * @return true if success, false otherwise
 */
boolean_t tosdb_cache_close(tosdb_cache_t* cache);

/**
 * @brief gets tosdb cache item
 * @param cache tosdb cache
 * @param key cache key
 * @return tosdb cache item if success, NULL otherwise
 */
const tosdb_cache_key_t* tosdb_cache_get(tosdb_cache_t* cache, tosdb_cache_key_t* key);

/**
 * @brief puts tosdb cache item
 * @param cache tosdb cache
 * @param key cache key
 * @return true if success, false otherwise
 */
boolean_t tosdb_cache_put(tosdb_cache_t* cache, tosdb_cache_key_t* key);

#ifdef __cplusplus
}
#endif

#endif
