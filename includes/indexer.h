/**
 * @file indexer.h
 * @brief indexer interface
 *
 * allow indexing of @ref linkedlist_t
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___INDEXER_H
/*! prevent duplicate header error macro */
#define ___INDEXER_H 0

#include <types.h>
#include <memory.h>
#include <iterator.h>

/**
 * @enum index_key_search_criteria_t
 * @brief key compression criteria for searching index.
 */
typedef enum index_key_search_criteria_t {
    INDEXER_KEY_COMPARATOR_CRITERIA_NULL,
    INDEXER_KEY_COMPARATOR_CRITERIA_LESS,
    INDEXER_KEY_COMPARATOR_CRITERIA_LESSOREQUAL,
    INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL,
    INDEXER_KEY_COMPARATOR_CRITERIA_EQUALORGREATER,
    INDEXER_KEY_COMPARATOR_CRITERIA_GREATER,
    INDEXER_KEY_COMPARATOR_CRITERIA_BETWEEN
} index_key_search_criteria_t;


typedef int8_t (* index_key_comparator_f)(const void* key1, const void* key2);

typedef struct index_t {
    memory_heap_t*         heap;
    void*                  metadata;
    index_key_comparator_f comparator;
    int8_t (* insert)(struct index_t* idx, void* key, void* data, void** removed_data);
    int8_t (*delete)(struct index_t* idx, void* key, void** deleted_data);
    iterator_t*  (* search)(struct index_t* idx, void* key1, void* key2, index_key_search_criteria_t criteria);
    iterator_t* (* create_iterator)(struct index_t* idx);
} index_t;

typedef void * (* indexer_key_creator_f)(void* key, void* keyarg);

/**
 * @typedef indexer_t
 * @brief implicit indexer type.
 */
typedef void * indexer_t;

indexer_t indexer_create_with_heap(memory_heap_t* heap);
#define indexer_create() indexer_create_ext(NULL);
int8_t      indexer_destroy(indexer_t idxer);
int8_t      indexer_register_index(indexer_t idxer, uint64_t idx_id, index_t* idx, indexer_key_creator_f key_creator, void* keyarg);
int8_t      indexer_index(indexer_t idxer, void* key, void* data);
void*       indexer_delete(indexer_t idxer, void* key);
iterator_t* indexer_search(indexer_t idxer, uint64_t idx_id, void* key1, void* key2, index_key_search_criteria_t criteria);


#endif
