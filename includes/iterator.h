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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct iterator_t
 * @brief iterator interface struct
 *
 * the implementation of destroy, next, end_of_iterator and get_item is mendotary.
 */
typedef struct iterator_t {
    void* metadata; ///< iterable struct's metadata
    int8_t (* destroy)(struct iterator_t* iter); ///< destroys itself
    struct iterator_t* (* next)(struct iterator_t* iter); ///< travels to next item in iterator
    int8_t (* end_of_iterator)(struct iterator_t* iter); ///< checks iterator ended
    const void* (* get_item)(struct iterator_t* iter); ///< returns current item data in iterator
    const void* (* delete_item)(struct iterator_t* iter); ///< deletes current item and moves to next
    const void* (* get_extra_data)(struct iterator_t* iter); ///< if underlaying iterable has extra data for current item, returns them
}iterator_t; ///< short hand for struct

#ifdef __cplusplus
}
#endif

#endif
