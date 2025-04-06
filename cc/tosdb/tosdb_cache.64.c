/**
 * @file tosdb_cache.64.c
 * @brief tosdb cache interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb_cache.h>
#include <tosdb/tosdb_internal.h>
#include <cache.h>
#include <logging.h>
#include <logging.h>
#include <xxhash.h>

/*! module name */
MODULE("turnstone.kernel.db");

/**
 * @struct tosdb_cache_t
 * @brief tosdb cache structure
 */
struct tosdb_cache_t {
    tosdb_cache_config_t config; ///< cache configuration
    cache_t*             bloomfilter_cache; ///< bloomfilter cache
    cache_t*             index_data_cache; ///< index data cache
    cache_t*             valuelog_cache; ///< valuelog cache
};

/**
 * @brief tosdb cache key generator
 * @param item item to create key
 * @return key which is generated from item with xxhash64 algorithm
 */
uint64_t tosdb_cache_key_generator(const void* item);

/**
 * @brief tosdb cache key comparator
 * @param item1 item1 to compare
 * @param item2 item2 to compare
 * @return 0 if item1 == item2, -1 if item1 < item2, 1 if item1 > item2
 */
int8_t tosdb_cache_key_comparator(const void* item1, const void* item2);

/**
 * @brief tosdb cache item key destroyer
 * @param key key of item
 * @param item item to destroy
 * @return true if item is destroyed, false otherwise
 */
boolean_t tosdb_cache_item_key_destroyer(const void* key, const void* item);



uint64_t tosdb_cache_key_generator(const void* item) {
    const tosdb_cache_key_t* key = item;

    xxhash64_context_t* hctx = xxhash64_init(0);

    xxhash64_update(hctx, &key->database_id, 8);
    xxhash64_update(hctx, &key->table_id, 8);
    xxhash64_update(hctx, &key->index_id, 8);
    xxhash64_update(hctx, &key->level, 8);
    xxhash64_update(hctx, &key->sstable_id, 8);

    return xxhash64_final(hctx);
}

int8_t tosdb_cache_key_comparator(const void* item1, const void* item2) {
    const tosdb_cache_key_t* key1 = item1;
    const tosdb_cache_key_t* key2 = item2;

    if(key1->database_id < key2->database_id) {
        return -1;
    }

    if(key1->database_id > key2->database_id) {
        return 1;
    }

    if(key1->table_id < key2->table_id) {
        return -1;
    }

    if(key1->table_id > key2->table_id) {
        return 1;
    }

    if(key1->index_id < key2->index_id) {
        return -1;
    }

    if(key1->index_id > key2->index_id) {
        return 1;
    }

    if(key1->level < key2->level) {
        return -1;
    }

    if(key1->level > key2->level) {
        return 1;
    }

    if(key1->sstable_id < key2->sstable_id) {
        return -1;
    }

    if(key1->sstable_id > key2->sstable_id) {
        return 1;
    }

    return 0;
}

boolean_t tosdb_cache_item_key_destroyer(const void* key, const void* item) {
    UNUSED(key);

    const tosdb_cache_key_t* ckey = item;

    if(ckey->type == TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER) {
        tosdb_cached_bloomfilter_t* c_bf = (tosdb_cached_bloomfilter_t*)item;
        memory_free(c_bf->first_key);
        memory_free(c_bf->last_key);
        memory_free(c_bf->secondary_first_key);
        memory_free(c_bf->secondary_last_key);
        bloomfilter_destroy(c_bf->bloomfilter);
        memory_free(c_bf);
    } else if(ckey->type == TOSDB_CACHE_ITEM_TYPE_INDEX_DATA) {
        tosdb_cached_index_data_t* c_id = (tosdb_cached_index_data_t*)item;

        memory_free(c_id->index_items[0]);
        memory_free(c_id->index_items);
        memory_free(c_id);
    } else if(ckey->type == TOSDB_CACHE_ITEM_TYPE_VALUELOG) {
        tosdb_cached_valuelog_t* c_vl = (tosdb_cached_valuelog_t*)item;
        buffer_destroy(c_vl->values);
        memory_free(c_vl);
    }

    return true;
}

tosdb_cache_t* tosdb_cache_new(tosdb_cache_config_t* config) {
    if(!config) {
        return NULL;
    }

    tosdb_cache_t* cache = memory_malloc(sizeof(tosdb_cache_t));

    if(!cache) {
        return NULL;
    }

    memory_memcopy(config, cache, sizeof(tosdb_cache_config_t));

    cache_config_t cc = {0};
    cc.item_key_destroyer = tosdb_cache_item_key_destroyer;
    cc.key_comparator = tosdb_cache_key_comparator;
    cc.key_generator = tosdb_cache_key_generator;

    cc.hard_limit = config->bloomfilter_size;
    cc.soft_limit = cc.hard_limit / 2;
    cache->bloomfilter_cache = cache_new(&cc);

    if(!cache->bloomfilter_cache) {
        memory_free(cache);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create bloomfilter cache");

        return NULL;
    }

    cc.hard_limit = config->index_data_size;
    cc.soft_limit = cc.hard_limit / 2;
    cache->index_data_cache = cache_new(&cc);

    if(!cache->index_data_cache) {
        cache_destroy(cache->bloomfilter_cache);
        memory_free(cache);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index data cache");

        return NULL;
    }

    cc.hard_limit = config->valuelog_size;
    cc.soft_limit = cc.hard_limit / 2;
    cache->valuelog_cache = cache_new(&cc);

    if(!cache->valuelog_cache) {
        cache_destroy(cache->index_data_cache);
        cache_destroy(cache->bloomfilter_cache);
        memory_free(cache);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create valuelog cache");

        return NULL;
    }

    return cache;
}

boolean_t tosdb_cache_close(tosdb_cache_t* cache) {
    if(!cache) {
        return false;
    }

    cache_destroy(cache->bloomfilter_cache);
    cache_destroy(cache->index_data_cache);
    cache_destroy(cache->valuelog_cache);

    memory_free(cache);

    return true;
}

const tosdb_cache_key_t* tosdb_cache_get(tosdb_cache_t* cache, tosdb_cache_key_t* key) {
    if(!cache || !key) {
        return NULL;
    }

    switch(key->type) {
    case TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER:
        return cache_get(cache->bloomfilter_cache, key);
    case TOSDB_CACHE_ITEM_TYPE_INDEX_DATA:
        return cache_get(cache->index_data_cache, key);
    case TOSDB_CACHE_ITEM_TYPE_VALUELOG:
        return cache_get(cache->valuelog_cache, key);
        break;
    default:
        break;
    }

    return NULL;
}

boolean_t tosdb_cache_put(tosdb_cache_t* cache, tosdb_cache_key_t* key) {
    if(!cache || !key) {
        return false;
    }

    switch(key->type) {
    case TOSDB_CACHE_ITEM_TYPE_BLOOMFILTER:
        return cache_put_item_as_key(cache->bloomfilter_cache, key, key->data_size);
    case TOSDB_CACHE_ITEM_TYPE_INDEX_DATA:
        return cache_put_item_as_key(cache->index_data_cache, key, key->data_size);
    case TOSDB_CACHE_ITEM_TYPE_VALUELOG:
        return cache_put_item_as_key(cache->valuelog_cache, key, key->data_size);
        break;
    default:
        break;
    }

    return false;
}
