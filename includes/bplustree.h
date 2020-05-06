/**
 * @file bplustree.h
 * @brief b+ tree indexer interface
 */
#ifndef ___BPLUSTREE_H
/*! prevent duplicate header error macro */
#define ___BPLUSTREE_H 0
#include <types.h>
#include <memory.h>
#include <indexer.h>
#include <linkedlist.h>

/*! b+ tree iterator implicit type*/
typedef void* bplustree_iterator_t;

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

/**
 * @brief creates b+ tree iterator
 * @param[in]  idx source of b+ tree
 * @return iterator
 *
 * the iterator travels only leaf nodes.
 */
bplustree_iterator_t bplustree_iterator_create(index_t* idx);

/**
 * @brief destroys the iterator
 * @param[in]  iterator iterator to destroy
 * @return  0 if succeed
 */
int8_t bplustree_iterator_destroy(bplustree_iterator_t iterator);

/**
 * @brief returns 0 at and of tree
 * @param[in]  iterator iterator to check
 * @return   0 if end of tree.
 */
int8_t bplustree_iterator_end_of_index(bplustree_iterator_t iterator);

/**
 * @brief fetches next key/value
 * @param[in]  iterator iterator to travel
 * @return   itself
 */
bplustree_iterator_t bplustree_iterator_next(bplustree_iterator_t iterator);

/**
 * @brief returns current key at iterator.
 * @param[in] iterator iterator to get key
 * @return the key.
 */
void* bplustree_iterator_get_key(bplustree_iterator_t iterator);

/**
 * @brief returns current data at iterator.
 * @param[in] iterator iterator to get data
 * @return the data.
 */
void* bplustree_iterator_get_data(bplustree_iterator_t iterator);

#endif
