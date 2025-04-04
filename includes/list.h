/**
 * @file 188.cpp
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___LIST_H
/*! prevent duplicate header error macro */
#define ___LIST_H 0

#include <types.h>
#include <memory.h>
#include <indexer.h>
#include <iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief linked list operation enum
 *
 * inserting/deleting will be performed with this values.
 */
typedef enum list_insert_delete_at_t {
    LIST_INSERT_AT_HEAD, ///< insert data at head of list
    LIST_INSERT_AT_TAIL, ///< insert data at tail of list
    LIST_INSERT_AT_POSITION, ///< insert data at given position (start at 0) of list
    LIST_INSERT_AT_SORTED, ///< insert data into list with sorted
    LIST_INSERT_AT_INDEXED, ///< insert data into list with indexed
    LIST_DELETE_AT_HEAD, ///< delete data from head of list
    LIST_DELETE_AT_TAIL, ///< delete data from tail of list
    LIST_DELETE_AT_FINDBY, ///< delete data from list with searching inside the list
    LIST_DELETE_AT_POSITION ///< delete data at given position (start at 0) of list
}list_insert_delete_at_t;

/*! insert data at anywhere of list (at tail for o(1)) */
#define LIST_INSERT_AT_ANYWHERE LIST_INSERT_AT_TAIL

/**
 * @brief linked list type
 *
 * used only information
 */
typedef enum list_type_t {
    LIST_TYPE_LIST = 1 << 0, ///< normal list
    LIST_TYPE_SORTEDLIST = 1 << 1, ///< sorted list
    LIST_TYPE_QUEUE = 1 << 2, ///< queue
    LIST_TYPE_STACK = 1 << 3, ///< stack
    LIST_TYPE_INDEXEDLIST = 1 << 8, ///< indexed list
    LIST_TYPE_LINKED = 1 << 9, ///< linked list
    LIST_TYPE_ARRAY = 1 << 10, ///< array list
}list_type_t;

/**
 * @brief linked list destroy type
 */
typedef enum list_destroy_type_t {
    LIST_DESTROY_WITHOUT_DATA, ///< destroy linked list without its data
    LIST_DESTROY_WITH_DATA ///< destroy linked list with its data
}list_destroy_type_t;

/**
 * @typedef list_t
 * @brief linked list implicit type
 */
typedef struct list_t list_t;

/**
 * @typedef list_item_t
 * @brief linked list item implicit type
 */
typedef struct list_item_t list_item_t;

/**
 * @brief comparing given to data
 * @param[in] data1
 * @param[in] data2
 * @return <0 if, data1 lt data2, 0 if data1 eq data2, >0 if data1 gt data2
 *
 * compares data1 and data2. this comparator used when linked list sorted.
 */
typedef int8_t (* list_data_comparator_f)(const void* data1, const void* data2);

int8_t list_default_data_comparator(const void* data1, const void* data2);
#define list_integer_comparator list_default_data_comparator
int8_t list_string_comprator(const void* data1, const void* data2);

/**
 * @brief linked list creator
 * @param[in] heap @ref memory_heap_t the heap where linked list will be at.
 * @param[in] type @ref list_type_t linked list type
 * @param[in] comparator @ref list_data_comparator_f data comparator used at sorted list
 * @param[in] indexer @ref indexer_t index linked list nodes.
 * @return @ref list_t
 *
 * creates linked list with given arguments. for each type of linked list
 * there is a macro. do not use this method directly.
 *
 * if heap  is null then linked list created at default heap.
 */
list_t* list_create_with_type(memory_heap_t* heap, list_type_t type,
                              list_data_comparator_f comparator, indexer_t* indexer);

/**
 * @brief returns list's heap
 * @param[in]  list list to be get heap
 * @return @ref memory_heap_t
 */
memory_heap_t* list_get_heap(list_t* list);

/**
 * @brief sets list's capacity
 * @param[in]  list       list to be modified
 * @param  capacity new capacity
 * @return 0 on success
 *
 * if list is null then this method returns -1.
 * if new capacity is less than current size of list then this method returns -1.
 * if list is not array list then this method returns -2.
 */
int8_t list_set_capacity(list_t* list, size_t capacity);

/**
 * @brief updates list's comparator and returns the old one.
 * @param[in]  list       list to be modified
 * @param  comparator new comparator
 * @return old comparator
 */
list_data_comparator_f list_set_comparator(list_t* list, list_data_comparator_f comparator);

/**
 * @brief creates a normal linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @return @ref list_t
 */
#define list_create_list_with_heap(h) list_create_with_type(memory_get_heap(h), LIST_TYPE_LIST, NULL, NULL)
/**
 * @brief creates a normal linked list at default heap heap
 * @return @ref list_t
 */
#define list_create_list() list_create_with_type(memory_get_heap(NULL), LIST_TYPE_LIST, NULL, NULL)

/**
 * @brief creates a sorted linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @param[in] c @ref list_data_comparator_f comparator used sorting list.
 * @return @ref list_t
 */
#define list_create_sortedlist_with_heap(h, c) list_create_with_type(memory_get_heap(h), LIST_TYPE_SORTEDLIST, c, NULL)
/**
 * @brief creates a sorted linked list at default heap
 * @param[in] c @ref list_data_comparator_f comparator used sorting list.
 * @return @ref list_t
 */
#define list_create_sortedlist(c) list_create_with_type(memory_get_heap(NULL), LIST_TYPE_SORTEDLIST, c, NULL)

/**
 * @brief creates a indexed linked list at heap
 * @param[in] h @ref memory_heap_t the heap of linked list.
 * @param[in] i @ref indexer_t indexer used sorting list.
 * @return @ref list_t
 */
#define list_create_indexedlist_with_heap(h, i) list_create_with_type(memory_get_heap(h), LIST_TYPE_INDEXEDLIST, NULL, i)
/**
 * @brief creates a indexed linked list at default heap
 * @param[in] i @ref indexer_t indexer used sorting list.
 * @return @ref list_t
 */
#define list_create_indexedlist(i) list_create_with_type(memory_get_heap(NULL), LIST_TYPE_INDEXEDLIST, NULL, i)

/**
 * @brief creates a queue at heap
 * @param[in] h @ref memory_heap_t the heap of queue.
 * @return @ref list_t
 */
#define list_create_queue_with_heap(h) list_create_with_type(memory_get_heap(h), LIST_TYPE_QUEUE, NULL, NULL)
/**
 * @brief creates a queue at default heap heap
 * @return @ref list_t
 */
#define list_create_queue() list_create_with_type(memory_get_heap(NULL), LIST_TYPE_QUEUE, NULL, NULL)

/**
 * @brief creates a stack at heap
 * @param[in] h @ref memory_heap_t the heap of queue.
 * @return @ref list_t
 */
#define list_create_stack_with_heap(h) list_create_with_type(memory_get_heap(h), LIST_TYPE_STACK, NULL, NULL)
/**
 * @brief creates a stack at default heap heap
 * @return @ref list_t
 */
#define list_create_stack() list_create_with_type(memory_get_heap(NULL), LIST_TYPE_STACK, NULL, NULL)

typedef int8_t (*list_item_destroyer_callback_f)(memory_heap_t* heap, void* data);

/**
 * @brief destroys linked list
 * @param[in] list @ref list_t* the list to be destoyed
 * @param[in] type @ref list_destroy_type_t the type with
 * @param[in] destroyer @ref list_item_destroyer_callback_f destroyer callback, it frees list item
 * @return 0 on success.
 *
 * this method destroys only the linked list with choice of preserving data.
 * if you do not destroy the data a memory leak will be happened if without data
 * destroying
 */
uint8_t list_destroy_with_type(list_t* list, list_destroy_type_t type, list_item_destroyer_callback_f destroyer);

/*! destroy without data macro */
#define list_destroy(l) list_destroy_with_type(l, LIST_DESTROY_WITHOUT_DATA, NULL)
/*! destroy with data macro */
#define list_destroy_with_data(l) list_destroy_with_type(l, LIST_DESTROY_WITH_DATA, NULL)

/**
 * @brief returns item count at linked list
 * @param[in]  list @ref list_t* the list whose size will be returned
 * @return @ref size_t linked list size
 */
size_t list_size(const list_t* list);

/**
 * @brief return data inside implicit list item type
 * @param[in] list_item list item
 * @return data inside the list.
 */
const void* list_get_data_from_listitem(list_item_t* list_item);

/**
 * @brief general method for inserting or deleting data from list types.
 * @param[in] list  the list
 * @param[in] data  the data
 * @param[in] where where and how the data will be inserted
 * @param[in] position if data will be added by position
 * @return insertation location
 */
size_t list_insert_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);

/**
 * @brief general method for inserting or deleting data from list types.
 * @param[in] list  the list
 * @param[in] data  the data
 * @param[in] where where and how the data will be inserted
 * @param[in] position if data will be deleted by position
 * @return the deleted data
 */
const void* list_delete_at(list_t* list, const void* data, list_insert_delete_at_t where, size_t position);

list_item_t* list_insert_at_head_and_get_list_item(list_t* list, const void* data);
boolean_t    list_move_item_to_head(list_t* list, list_item_t* item);
boolean_t    list_delete_list_item(list_t* list, list_item_t* item);

/*! insert data with position into list */
#define list_insert_at_position(l, d, p ) list_insert_at(l, d, LIST_INSERT_AT_POSITION, p)
/*! delete data with position from list */
#define list_delete_at_position(l, p ) list_delete_at(l, NULL, LIST_DELETE_AT_POSITION, p)

/*! delete data at tail (end) of list */
#define list_delete_at_tail(l) list_delete_at(l, NULL, LIST_DELETE_AT_TAIL, 0)

/*! insert data into normal list */
#define list_list_insert(l, d) list_insert_at(l, d, LIST_INSERT_AT_ANYWHERE, 0)
/*! delete and get data from normal list */
#define list_list_delete(l, d) list_delete_at(l, d, LIST_DELETE_AT_FINDBY, 0)

/*! insert data into sorted list */
#define list_sortedlist_insert(l, d) list_insert_at(l, d, LIST_INSERT_AT_SORTED, 0)
/*! delete and get data from sorted list */
#define list_sortedlist_delete(l, d) list_delete_at(l, d, LIST_DELETE_AT_FINDBY, 0)

/*! insert data into indexed list */
#define list_indexedlist_insert(l, d) list_insert_at(l, d, LIST_INSERT_AT_INDEXED, 0)
/*! delete and get data from indexed list */
#define list_indexedlist_delete(l, d) list_delete_at(l, d, LIST_DELETE_AT_FINDBY, 0)

/*! insert data into queue */
#define list_queue_push(l, d) list_insert_at(l, d, LIST_INSERT_AT_TAIL, 0)
/*! delete and get data from queue */
#define list_queue_pop(l) list_delete_at(l, NULL, LIST_DELETE_AT_HEAD, 0)

/*! insert data into stack */
#define list_stack_push(l, d) list_insert_at(l, d, LIST_INSERT_AT_HEAD, 0)
/*! delete and get data from stack */
#define list_stack_pop(l) list_delete_at(l, NULL, LIST_DELETE_AT_HEAD, 0)

/*! insert data into head */
#define list_insert_at_head(l, d) list_insert_at(l, d, LIST_INSERT_AT_HEAD, 0)

/**
 * @brief returns position of given data.
 * @param[in]  list list to search
 * @param[in]  data data to search
 * @param[position]  position the position of data if found
 * @return  0 if data found, else -1.
 */
int8_t list_get_position(list_t* list, const void* data, size_t* position);

#define list_contains(l, d)  list_get_position(l, d, NULL)
/**
 * @brief returns position of given data.
 * @param  list list to search
 * @param  position position of data
 * @return data if found or null
 */
const void* list_get_data_at_position(list_t* list, size_t position);

#define list_queue_peek(l) list_get_data_at_position(l, 0);
#define list_stack_peek(l) list_get_data_at_position(l, 0);

/**
 * @brief duplicates list at the given heap
 * @param[in]  heap the heap where the list will be created
 * @param[in]  list source list
 * @return  a new list at heap
 *
 * if heap is NULL then the new heap is same as source list's heap.
 */
list_t* list_duplicate_list_with_heap(memory_heap_t* heap, list_t* list);
/*! duplicate linked list with same as heap at source list */
#define list_duplicate_list(l) list_duplicate_list_with_heap(NULL, l);

/**
 * @brief creates an iterator from the list
 * @param[in]  list source list
 * @return      the iterator
 *
 * the returned type is implicit. see also list_iterator_internal_t
 * iterator is created at the heap of list.
 */
iterator_t* list_iterator_create(list_t* list);

/**
 * @brief sets equality comparator for list
 * @param[in]  list source list
 * @param[in]  comparator comparator function
 * @return      0 on success
 **/
int8_t list_set_equality_comparator(list_t* list, list_data_comparator_f comparator);

/**
 * @brief merge given list into self list
 * @param[in]  self source list
 * @param[in]  list destination list
 * @return      0 on success
 **/
int8_t list_merge(list_t* self, list_t* list);

#ifdef __cplusplus
}
#endif

#endif
