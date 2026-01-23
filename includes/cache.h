/**
 * @file cache.h
 * @brief cache interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CACHE_H
#define ___CACHE_H 0

#include <types.h>
#include <hashmap.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! @enum cache_policy_t
 * @brief Defines the policy for managing cache eviction.
 */
typedef enum cache_policy_t {
    CACHE_POLICY_COUNT, ///< @brief Cache eviction based on the number of items.
    CACHE_POLICY_SIZE, ///< @brief Cache eviction based on the total size of items.
} cache_policy_t;

/*! @typedef cache_item_key_destroyer_f
 * @brief Function pointer for destroying a cache item's key and potentially the item itself.
 *
 * This callback is invoked when an item is evicted from the cache or the cache is destroyed.
 * It allows for custom cleanup of the key and/or the associated item.
 *
 * @param key Pointer to the key of the item being destroyed.
 * @param item Pointer to the item being destroyed.
 * @return TRUE on successful destruction, FALSE otherwise.
 */
typedef boolean_t (*cache_item_key_destroyer_f)(const void* key, const void* item);

/*! @struct cache_config_t
 * @brief Configuration structure for creating a new cache instance.
 *
 * This structure encapsulates all necessary parameters for initializing a cache,
 * including its eviction policy, limits, and custom callback functions for key
 * generation, comparison, and destruction.
 */
typedef struct cache_config_t {
    cache_policy_t             policy; ///< @brief The cache eviction policy to use.
    uint64_t                   hard_limit; ///< @brief The absolute maximum limit (count or size) for the cache.
    uint64_t                   soft_limit; ///< @brief The soft limit, triggering eviction when exceeded.
    hashmap_key_generator_f    key_generator; ///< @brief Function to generate a hash for a cache key.
    hashmap_key_comparator_f   key_comparator; ///< @brief Function to compare two cache keys.
    cache_item_key_destroyer_f item_key_destroyer; ///< @brief Function to destroy a cache item's key and item.
} cache_config_t;

/*! @typedef cache_t
 * @brief Opaque structure representing a cache instance.
 *
 * Users interact with the cache through pointers to this opaque type.
 */
typedef struct cache_t cache_t;

/**
 * @brief Creates and initializes a new cache instance.
 *
 * This function allocates memory for a new cache and configures it according to
 * the provided `cache_config_t` structure.
 *
 * @param config Pointer to a `cache_config_t` structure containing the cache's configuration.
 *               The `config` structure itself is not copied, so its contents should remain valid
 *               for the lifetime of this call.
 * @return A pointer to the newly created `cache_t` instance on success, or NULL on failure.
 */
cache_t* cache_new(cache_config_t* config);

/**
 * @brief Destroys a cache instance and frees all associated memory.
 *
 * This function deallocates the cache structure and all items stored within it.
 * If an `item_key_destroyer` was provided in the `cache_config_t`, it will be
 * called for each item to allow for custom cleanup.
 *
 * @param cache Pointer to the `cache_t` instance to destroy.
 * @return TRUE on successful destruction, FALSE otherwise.
 */
boolean_t cache_destroy(cache_t* cache);

/**
 * @brief Inserts or updates an item in the cache.
 *
 * This function adds a new item to the cache or updates an existing one if a
 * matching key is found. The `size` parameter is used if the cache policy is
 * `CACHE_POLICY_SIZE`. If the cache exceeds its soft limit after insertion,
 * eviction may occur.
 *
 * @param cache Pointer to the `cache_t` instance.
 * @param key Pointer to the key associated with the item. The cache will store
 *            a copy or reference to this key based on its internal implementation.
 * @param item Pointer to the item to be stored. The cache will store a copy or
 *             reference to this item based on its internal implementation.
 * @param size The size of the item in bytes. This parameter is only relevant
 *             if the cache's policy is `CACHE_POLICY_SIZE`. For `CACHE_POLICY_COUNT`,
 *             it can be set to 1 or any other value.
 * @return TRUE on successful insertion/update, FALSE on failure (e.g., out of memory).
 */
boolean_t cache_put(cache_t* cache, const void* key, const void* item, uint64_t size);

/**
 * @brief Macro to insert an item into the cache when the policy is `CACHE_POLICY_COUNT`.
 *
 * This macro simplifies calling `cache_put` by automatically setting the size to 1,
 * which is appropriate for count-based eviction.
 *
 * @param c Pointer to the `cache_t` instance.
 * @param k Pointer to the key.
 * @param i Pointer to the item.
 */
#define cache_put_by_count(c, k, i) cache_put(c, k, i, 1)

/**
 * @brief Macro to insert an item into the cache where the item itself serves as the key.
 *
 * This macro is useful when the item's address or content can directly be used as its key.
 *
 * @param c Pointer to the `cache_t` instance.
 * @param i Pointer to the item, which also serves as the key.
 * @param s The size of the item in bytes.
 */
#define cache_put_item_as_key(c, i, s) cache_put(c, i, i, s)

/**
 * @brief Retrieves an item from the cache using its key.
 *
 * If the item is found, it is returned. Depending on the cache's internal
 * eviction strategy (e.g., LRU), accessing an item might update its position
 * in the eviction order.
 *
 * @param cache Pointer to the `cache_t` instance.
 * @param key Pointer to the key of the item to retrieve.
 * @return A `const void*` pointer to the retrieved item on success, or NULL if the item is not found.
 *         The returned pointer points to data owned by the cache and should not be freed by the caller.
 */
const void* cache_get(cache_t* cache, const void* key);

#ifdef __cplusplus
}
#endif

#endif
