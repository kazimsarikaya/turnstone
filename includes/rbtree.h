/**
 * @file rbltree.h
 * @brief tosdb internal interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___RBLTREE_H
#define ___RBLTREE_H 0

#include <indexer.h>

index_t* rbtree_create_index_with_heap(memory_heap_t* heap, index_key_comparator_f comparator);
#define rbtree_create_index(c) rbtree_create_index_with_heap(NULL, c)
int8_t rbtree_destroy_index(index_t* idx);

#endif
