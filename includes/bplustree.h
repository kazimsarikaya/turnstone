/**
 * @file bplustree.h
 * @brief b+ tree indexer interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___BPLUSTREE_H
/*! prevent duplicate header error macro */
#define ___BPLUSTREE_H 0
#include <types.h>
#include <memory.h>
#include <indexer.h>
#include <list.h>
#include <iterator.h>

/**
 * @brief creates b+ tree index implementation
 * @param  heap          heap to use
 * @param  max_key_count maximum key count at each tree node
 * @param  comparator    key comparator
 * @param  unique        if unique flag set remove and insert key,value
 * @return               index interface
 */
index_t* bplustree_create_index_with_heap_and_unique(memory_heap_t* heap, uint64_t max_key_count,
                                                     index_key_comparator_f comparator, boolean_t unique);

/**
 * @brief creates b+ tree index with default heap
 * @param[in]  mkc max key count
 * @param[in]  c   comparator
 * @return     b+ tree  index
 */
#define bplustree_create_index(mkc, c) bplustree_create_index_with_heap_and_unique(NULL, mkc, c, false)

/**
 * @brief creates b+ tree index with default heap
 * @param[in]  mkc max key count
 * @param[in]  c   comparator
 * @param[in]  u unique flag
 * @return     b+ tree  index
 */
#define bplustree_create_index_with_unique(mkc, c, u) bplustree_create_index_with_heap_and_unique(NULL, mkc, c, u)

/**
 * @brief creates b+ tree index with default heap
 * @param[in]  h  heap to use
 * @param[in]  mkc max key count
 * @param[in]  c   comparator
 * @return     b+ tree  index
 */
#define bplustree_create_index_with_heap(h, mkc, c) bplustree_create_index_with_heap_and_unique(h, mkc, c, false)

/**
 * @brief destroys index
 * @param[in]  idx index to be destroyed
 * @return     0 if successed.
 *
 * destroys only b+tree not data. if data will not be destroyed a memory leak will be happened.
 */
int8_t bplustree_destroy_index(index_t* idx);

/**
 * @brief sets a comparator for unique subpart for non unique index
 * @param[in]  idx        index
 * @param[in]  comparator comparator
 * @return     0 if successed.
 */
int8_t bplustree_set_comparator_for_unique_subpart_for_non_unique_index(index_t* idx, index_key_comparator_f comparator);

typedef int8_t (*bplustree_key_destroyer_f)(memory_heap_t* heap, void* key);

/**
 * @brief sets a key destroyer for index
 * @param[in]  idx       index
 * @param[in]  destroyer destroyer
 * @return     0 if successed.
 */
int8_t bplustree_set_key_destroyer(index_t* idx, bplustree_key_destroyer_f destroyer);

typedef int8_t (*bplustree_key_cloner_f)(memory_heap_t* heap, const void* key, void** cloned_key);

/**
 * @brief sets a key cloner for index
 * @param[in]  idx    index
 * @param[in]  cloner cloner
 * @return     0 if successed.
 */
int8_t bplustree_set_key_cloner(index_t* idx, bplustree_key_cloner_f cloner);

#endif
