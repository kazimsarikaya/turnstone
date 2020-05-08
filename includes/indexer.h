/**
 * @file indexer.h
 * @brief indexer interface
 *
 * allow indexing of @ref linkedlist_t
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
typedef enum {
	INDEXER_KEY_COMPARATOR_CRITERIA_LESS=-1,
	INDEXER_KEY_COMPARATOR_CRITERIA_LESSOREQUAL,
	INDEXER_KEY_COMPARATOR_CRITERIA_EQUAL=0,
	INDEXER_KEY_COMPARATOR_CRITERIA_EQUALORGREATER,
	INDEXER_KEY_COMPARATOR_CRITERIA_GREATER=1
} index_key_search_criteria_t;


typedef int8_t (* index_key_comparator_f)(const void* key1, const void* key2);

typedef struct index {
	memory_heap_t* heap;
	void* metadata;
	index_key_comparator_f comparator;
	int8_t (* insert)(struct index* idx, void* key, void* data);
	int8_t (* delete)(struct index* idx, void* key, void** deleted_data);
	size_t (* search)(struct index* idx, void* key, void** result, index_key_search_criteria_t criteria);
	iterator_t* (*create_iterator)(struct index* idx);
} index_t;


typedef size_t (* index_search_f)(index_t* idx, void* key, void** result, index_key_search_criteria_t criteria);


/**
 * @typedef indexer_t
 * @brief implicit indexer type.
 */
typedef void* indexer_t;

uint8_t indexer_index(indexer_t idxer, void* key, void* data);
uint8_t indexer_delete(indexer_t idxer, void* key);
void* indexer_search(indexer_t idxer, void* key);


#endif
