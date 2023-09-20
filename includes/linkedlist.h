/**
 * @file linkedlist.h
 * @brief linked list interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___LINKEDLIST_H
/*! prevent duplicate header error macro */
#define ___LINKEDLIST_H 0

#include <types.h>
#include <memory.h>
#include <indexer.h>
#include <iterator.h>

/**
 * @brief linked list operation enum
 *
 * inserting/deleting will be performed with this values.
 */
typedef enum linkedlist_insert_delete_at_t {
    LINKEDLIST_INSERT_AT_HEAD, ///< insert data at head of list
    LINKEDLIST_INSERT_AT_TAIL, ///< insert data at tail of list
    LINKEDLIST_INSERT_AT_POSITION, ///< insert data at given position (start at 0) of list
    LINKEDLIST_INSERT_AT_SORTED, ///< insert data into list with sorted
    LINKEDLIST_INSERT_AT_INDEXED, ///< insert data into list with indexed
    LINKEDLIST_DELETE_AT_HEAD, ///< delete data from head of list
    LINKEDLIST_DELETE_AT_TAIL, ///< delete data from tail of list
    LINKEDLIST_DELETE_AT_FINDBY, ///< delete data from list with searching inside the list
    LINKEDLIST_DELETE_AT_POSITION ///< delete data at given position (start at 0) of list
}linkedlist_insert_delete_at_t;

/*! insert data at anywhere of list (at tail for o(1)) */
#define LINKEDLIST_INSERT_AT_ANYWHERE LINKEDLIST_INSERT_AT_TAIL

/**
 * @brief linked list type
 *
 * used only information
 */
typedef enum linkedlist_type_t {
    LINKEDLIST_TYPE_LIST = 1 << 0, ///< normal list
    LINKEDLIST_TYPE_SORTEDLIST = 1 << 1, ///< sorted list
    LINKEDLIST_TYPE_QUEUE = 1 << 2, ///< queue
    LINKEDLIST_TYPE_STACK = 1 << 3, ///< stack
    LINKEDLIST_TYPE_INDEXEDLIST = 1 << 8, ///< indexed list
}linkedlist_type_t;

/**
 * @brief linked list destroy type
 */
typedef enum linkedlist_destroy_type_t {
    LINKEDLIST_DESTROY_WITHOUT_DATA, ///< destroy linked list without its data
    LINKEDLIST_DESTROY_WITH_DATA ///< destroy linked list with its data
}linkedlist_destroy_type_t;

/**
 * @typedef linkedlist_t
 * @brief linked list implicit type
 */
typedef struct linkedlist_t linkedlist_t;

/**
 * @typedef linkedlist_item_t
 * @brief linked list item implicit type
 */
typedef struct linkedlist_item_t linkedlist_item_t;

/**
 * @brief comparing given to data
 * @param[in] data1
 * @param[in] data2
 * @return <0 if, data1 lt data2, 0 if data1 eq data2, >0 if data1 gt data2
 *
 * compares data1 and data2. this comparator used when linked list sorted.
 */
typedef int8_t (* linkedlist_data_comparator_f)(const void* data1, const void* data2);

int8_t linkedlist_default_data_comparator(const void* data1, const void* data2);
#define linkedlist_integer_comparator linkedlist_default_data_comparator
int8_t linkedlist_string_comprator(const void* data1, const void* data2);

/**
 * @brief linked list creator
 * @param[in] heap @ref memory_heap_t the heap where linked list will be at.
 * @param[in] type @ref linkedlist_type_t linked list type
 * @param[in] comparator @ref linkedlist_data_comparator_f data comparator used at sorted list
 * @param[in] indexer @ref indexer_t index linked list nodes.
 * @return @ref linkedlist_t
 *
 * creates linked list with given arguments. for each type of linked list
 * there is a macro. do not use this method directly.
 *
 * if heap  is null then linked list created at default heap.
 */
linkedlist_t* linkedlist_create_with_type(memory_heap_t* heap, linkedlist_type_t type,
                                          linkedlist_data_comparator_f comparator, indexer_t indexer);

memory_heap_t* linkedlist_get_heap(linkedlist_t* list);

/**
 * @brief updates list's comparator and returns the old one.
 * @param[in]  list       list to be modified
 * @param  comparator new comparator
 * @return old comparator
 */
linkedlist_data_comparator_f linkedlist_set_comparator(linkedlist_t* list, linkedlist_data_comparator_f comparator);

/**
 * @brief creates a normal linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_list_with_heap(h) linkedlist_create_with_type(memory_get_heap(h), LINKEDLIST_TYPE_LIST, NULL, NULL)
/**
 * @brief creates a normal linked list at default heap heap
 * @return @ref linkedlist_t
 */
#define linkedlist_create_list() linkedlist_create_with_type(memory_get_heap(NULL), LINKEDLIST_TYPE_LIST, NULL, NULL)

/**
 * @brief creates a sorted linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @param[in] c @ref linkedlist_data_comparator_f comparator used sorting list.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_sortedlist_with_heap(h, c) linkedlist_create_with_type(memory_get_heap(h), LINKEDLIST_TYPE_SORTEDLIST, c, NULL)
/**
 * @brief creates a sorted linked list at default heap
 * @param[in] c @ref linkedlist_data_comparator_f comparator used sorting list.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_sortedlist(c) linkedlist_create_with_type(memory_get_heap(NULL), LINKEDLIST_TYPE_SORTEDLIST, c, NULL)

/**
 * @brief creates a indexed linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @param[in] i @ref indexer_t indexer used sorting list.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_indexedlist_with_heap(h, i) linkedlist_create_with_type(memory_get_heap(h), LINKEDLIST_TYPE_INDEXEDLIST, NULL, i)
/**
 * @brief creates a indexed linked list at default heap
 * @param[in] i @ref indexer_t indexer used sorting list.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_indexedlist(i) linkedlist_create_with_type(memory_get_heap(NULL), LINKEDLIST_TYPE_INDEXEDLIST, NULL, i)

/**
 * @brief creates a queue at heap
 * @param[in] h @ref memory_heap_t the heap of queue.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_queue_with_heap(h) linkedlist_create_with_type(memory_get_heap(h), LINKEDLIST_TYPE_QUEUE, NULL, NULL)
/**
 * @brief creates a queue at default heap heap
 * @return @ref linkedlist_t
 */
#define linkedlist_create_queue() linkedlist_create_with_type(memory_get_heap(NULL), LINKEDLIST_TYPE_QUEUE, NULL, NULL)

/**
 * @brief creates a stack at heap
 * @param[in] h @ref memory_heap_t the heap of queue.
 * @return @ref linkedlist_t
 */
#define linkedlist_create_stack_with_heap(h) linkedlist_create_with_type(memory_get_heap(h), LINKEDLIST_TYPE_STACK, NULL, NULL)
/**
 * @brief creates a stack at default heap heap
 * @return @ref linkedlist_t
 */
#define linkedlist_create_stack() linkedlist_create_with_type(memory_get_heap(NULL), LINKEDLIST_TYPE_STACK, NULL, NULL)

/**
 * @brief destroys linked list
 * @param[in] list @ref linkedlist_t* the list to be destoyed
 * @param[in] type @ref linkedlist_destroy_type_t the type with
 * @return 0 on success.
 *
 * this method destroys only the linked list with choice of preserving data.
 * if you do not destroy the data a memory leak will be happened if without data
 * destroying
 */
uint8_t linkedlist_destroy_with_type(linkedlist_t* list, linkedlist_destroy_type_t type);

/*! destroy without data macro */
#define linkedlist_destroy(l) linkedlist_destroy_with_type(l, LINKEDLIST_DESTROY_WITHOUT_DATA)
/*! destroy with data macro */
#define linkedlist_destroy_with_data(l) linkedlist_destroy_with_type(l, LINKEDLIST_DESTROY_WITH_DATA)

/**
 * @brief returns item count at linked list
 * @param[in]  list @ref linkedlist_t* the list whose size will be returned
 * @return @ref size_t linked list size
 */
size_t linkedlist_size(const linkedlist_t* list);

/**
 * @brief return data inside implicit list item type
 * @param[in] list_item list item
 * @return data inside the list.
 */
const void* linkedlist_get_data_from_listitem(linkedlist_item_t* list_item);

/**
 * @brief general method for inserting or deleting data from list types.
 * @param[in] list  the list
 * @param[in] data  the data
 * @param[in] where where and how the data will be inserted
 * @param[in] position if data will be added by position
 * @return insertation location
 */
size_t linkedlist_insert_at(linkedlist_t* list, const void* data, linkedlist_insert_delete_at_t where, size_t position);

/**
 * @brief general method for inserting or deleting data from list types.
 * @param[in] list  the list
 * @param[in] data  the data
 * @param[in] where where and how the data will be inserted
 * @param[in] position if data will be deleted by position
 * @return the deleted data
 */
const void* linkedlist_delete_at(linkedlist_t* list, const void* data, linkedlist_insert_delete_at_t where, size_t position);

linkedlist_item_t* linkedlist_insert_at_head_and_get_linkedlist_item(linkedlist_t* list, const void* data);
boolean_t          linkedlist_move_item_to_head(linkedlist_t* list, linkedlist_item_t* item);
boolean_t          linkedlist_delete_linkedlist_item(linkedlist_t* list, linkedlist_item_t* item);

/*! insert data with position into list */
#define linkedlist_insert_at_position(l, d, p ) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_POSITION, p)
/*! delete data with position from list */
#define linkedlist_delete_at_position(l, p ) linkedlist_delete_at(l, NULL, LINKEDLIST_DELETE_AT_POSITION, p)

/*! delete data at tail (end) of list */
#define linkedlist_delete_at_tail(l) linkedlist_delete_at(l, NULL, LINKEDLIST_DELETE_AT_TAIL, 0)

/*! insert data into normal list */
#define linkedlist_list_insert(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_ANYWHERE, 0)
/*! delete and get data from normal list */
#define linkedlist_list_delete(l, d) linkedlist_delete_at(l, d, LINKEDLIST_DELETE_AT_FINDBY, 0)

/*! insert data into sorted list */
#define linkedlist_sortedlist_insert(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_SORTED, 0)
/*! delete and get data from sorted list */
#define linkedlist_sortedlist_delete(l, d) linkedlist_delete_at(l, d, LINKEDLIST_DELETE_AT_FINDBY, 0)

/*! insert data into indexed list */
#define linkedlist_indexedlist_insert(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_INDEXED, 0)
/*! delete and get data from indexed list */
#define linkedlist_indexedlist_delete(l, d) linkedlist_delete_at(l, d, LINKEDLIST_DELETE_AT_FINDBY, 0)

/*! insert data into queue */
#define linkedlist_queue_push(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_TAIL, 0)
/*! delete and get data from queue */
#define linkedlist_queue_pop(l) linkedlist_delete_at(l, NULL, LINKEDLIST_DELETE_AT_HEAD, 0)

/*! insert data into stack */
#define linkedlist_stack_push(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_HEAD, 0)
/*! delete and get data from stack */
#define linkedlist_stack_pop(l) linkedlist_delete_at(l, NULL, LINKEDLIST_DELETE_AT_HEAD, 0)

/*! insert data into head */
#define linkedlist_insert_at_head(l, d) linkedlist_insert_at(l, d, LINKEDLIST_INSERT_AT_HEAD, 0)

/**
 * @brief returns position of given data.
 * @param[in]  list list to search
 * @param[in]  data data to search
 * @param[position]  position the position of data if found
 * @return  0 if data found, else -1.
 */
int8_t linkedlist_get_position(linkedlist_t* list, const void* data, size_t* position);

#define linkedlist_contains(l, d)  linkedlist_get_position(l, d, NULL)
/**
 * @brief returns position of given data.
 * @param  list list to search
 * @param  position position of data
 * @return data if found or null
 */
const void* linkedlist_get_data_at_position(linkedlist_t* list, size_t position);

#define linkedlist_queue_peek(l) linkedlist_get_data_at_position(l, 0);
#define linkedlist_stack_peek(l) linkedlist_get_data_at_position(l, 0);

/**
 * @brief duplicates list at the given heap
 * @param[in]  heap the heap where the list will be created
 * @param[in]  list source list
 * @return  a new list at heap
 *
 * if heap is NULL then the new heap is same as source list's heap.
 */
linkedlist_t* linkedlist_duplicate_list_with_heap(memory_heap_t* heap, linkedlist_t* list);
/*! duplicate linked list with same as heap at source list */
#define linkedlist_duplicate_list(l) linkedlist_duplicate_list_with_heap(NULL, l);

/**
 * @brief creates an iterator from the list
 * @param[in]  list source list
 * @return      the iterator
 *
 * the returned type is implicit. see also linkedlist_iterator_internal_t
 * iterator is created at the heap of list.
 */
iterator_t* linkedlist_iterator_create(linkedlist_t* list);

int8_t linkedlist_set_equality_comparator(linkedlist_t* list, linkedlist_data_comparator_f comparator);

#endif
