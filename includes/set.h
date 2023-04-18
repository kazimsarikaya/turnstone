/**
 * @file set.h
 * @brief set interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SET_H
#define ___SET_H 0

#include <types.h>
#include <iterator.h>

/**
 * @brief set comparator
 * @param[in] item1 item 1
 * @param[in] item2 item 2
 */
typedef int8_t (*set_comparator_f)(const void* item1, const void* item2);

/*! set struct type */
typedef struct set_t set_t;

/**
 * @brief creates set
 * @param[in] cmp set comparator
 * @return set or null
 */
set_t* set_create(set_comparator_f cmp);

/**
 * @brief append the value to the set if not exists
 * @param[in] s the set
 * @param[in] value item to append
 * @return true if succeed.
 */
boolean_t set_append(set_t* s, void* value);

/**
 * @brief remove the value from the set
 * @param[in] s the set
 * @param[in] value item to remove
 * @return true if succeed.
 */
boolean_t set_remove(set_t* s, void* value);

/**
 * @brief check value if in the the set
 * @param[in] s the set
 * @param[in] value item to check
 * @return true if value in set.
 */
boolean_t set_exists(set_t* s, void* value);

/**
 * @brief set destroy callback
 * @param[in] item item to destroy
 * @return true if free succeed
 */
typedef boolean_t (*set_destroy_callback_f)(void* item);

/**
 * @brief destroys set
 * @param[in] s the set
 * @param[in] cb destroy callback which frees items.
 * @return true if succeed.
 */
boolean_t set_destroy_with_callback(set_t* s, set_destroy_callback_f cb);

/*! destroys set without callback */
#define set_destroy(s) set_destroy_with_callback(s, NULL);

/**
 * @brief creates set iterator
 * @param[in] s the set
 * @return iterator.
 */
iterator_t* set_create_iterator(set_t* s);

#endif
