/**
 * @file rbtree.h
 * @brief tosdb internal interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___RBLTREE_H
/*! macro for preventing second time include */
#define ___RBLTREE_H 0

#include <indexer.h>

/**
 * @brief create rbtree index
 * @param[in] heap memory heap to use
 * @param[in] comparator index key comparator
 * @return index or null
 */
index_t* rbtree_create_index_with_heap(memory_heap_t* heap, index_key_comparator_f comparator);

/*! macro for creating rbtree index with default heap */
#define rbtree_create_index(c) rbtree_create_index_with_heap(NULL, c)

/**
 * @brief destroy rbtree index
 * @param[in] idx index to destroy
 * @return 0 if success
 */
int8_t rbtree_destroy_index(index_t* idx);

#endif
