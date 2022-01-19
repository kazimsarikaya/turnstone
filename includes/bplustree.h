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
#include <linkedlist.h>
#include <iterator.h>

/**
 * @brief creates b+ tree index implementation
 * @param  heap          heap to use
 * @param  max_key_count maximum key count at each tree node
 * @param  comparator    key comparator
 * @return               index interface
 */
index_t* bplustree_create_index_with_heap(memory_heap_t* heap, uint64_t max_key_count,
                                          index_key_comparator_f comparator);

/**
 * @brief creates b+ tree index with default heap
 * @param[in]  mkc max key count
 * @param[in]  c   comparator
 * @return     b+ tree  index
 */
#define bplustree_create_index(mkc, c) bplustree_create_index_with_heap(NULL, mkc, c);

/**
 * @brief destroys index
 * @param[in]  idx index to be destroyed
 * @return     0 if successed.
 *
 * destroys only b+tree not data. if data will not be destroyed a memory leak will be happened.
 */
int8_t bplustree_destroy_index(index_t* idx);

#endif
