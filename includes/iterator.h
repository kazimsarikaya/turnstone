/**
 * @file iterator.h
 * @brief iterator interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___ITERATOR_H
/*! prevent duplicate header error macro */
#define ___ITERATOR_H 0

#include <types.h>

/**
 * @struct iterator
 * @brief iterator interface struct
 *
 * the implementation of destroy, next, end_of_iterator and get_item is mendotary.
 */
typedef struct iterator {
    void* metadata; ///< iterable struct's metadata
    int8_t (* destroy)(struct iterator* iter); ///< destroys itself
    struct iterator* (* next)(struct iterator* iter); ///< travels to next item in iterator
    int8_t (* end_of_iterator)(struct iterator* iter); ///< checks iterator ended
    void* (* get_item)(struct iterator* iter); ///< returns current item data in iterator
    void* (* delete_item)(struct iterator* iter); ///< deletes current item and moves to next
    void* (* get_extra_data)(struct iterator* iter); ///< if underlaying iterable has extra data for current item, returns them
}iterator_t; ///< short hand for struct

#endif
