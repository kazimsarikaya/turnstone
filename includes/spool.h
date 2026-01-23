/**
 * @file spool.h
 * @brief spool header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___SPOOL_H
#define ___SPOOL_H 0

#include <types.h>
#include <list.h>
#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque structure representing a single item within the spool.
 *
 * This structure holds metadata and references to the buffers associated
 * with a spooled item. Its internal details are hidden from the public API.
 */
typedef struct spool_item_t spool_item_t;

/**
 * @brief Initializes the spooling system.
 *
 * This function sets up the global spooling mechanism, including its
 * underlying memory heap and initial configuration. It must be called
 * before any other spool functions.
 *
 * @param spool_size The total size in bytes to allocate for the spool's memory heap.
 * @param spool_start The starting physical address for the spool's memory region.
 * @return 0 on success, or a non-zero error code on failure.
 */
int8_t spool_init(size_t spool_size, uint64_t spool_start);

/**
 * @brief Adds a new item to the spool with multiple associated buffers.
 *
 * This function creates a new `spool_item_t` and associates a variable
 * number of `buffer_t` instances with it. The buffers are typically
 * allocated from the spool's internal heap.
 *
 * @param name A null-terminated string representing the name of the spool item.
 * @param buf_cnt The number of `buffer_t` pointers that follow in the variadic arguments.
 * @param ... A variadic list of `buffer_t*` pointers to be associated with this spool item.
 * @return 0 on success, or a non-zero error code on failure.
 */
int8_t spool_add(const char_t* name, size_t buf_cnt, ...);

/**
 * @brief Retrieves a pointer to the memory heap used by the spooling system.
 *
 * This function allows external components to allocate memory from the
 * same heap used by the spool, which can be useful for related data structures.
 *
 * @return A pointer to the `memory_heap_t` instance used by the spool, or NULL if not initialized.
 */
memory_heap_t* spool_get_heap(void);

/**
 * @brief Retrieves a list of all items currently in the spool.
 *
 * The returned list contains pointers to `spool_item_t` instances.
 * The caller should not modify or free the returned list or its contents.
 *
 * @return A pointer to a `list_t` containing `spool_item_t*` elements, or NULL if the spool is empty or not initialized.
 */
list_t* spool_get_all(void);

/**
 * @brief Gets the name of a specific spool item.
 *
 * @param item Pointer to the `spool_item_t` whose name is to be retrieved.
 * @return A null-terminated string representing the name of the item, or NULL if the item is invalid.
 */
const char_t* spool_get_name(const spool_item_t* item);

/**
 * @brief Gets the number of buffers associated with a spool item.
 *
 * @param item Pointer to the `spool_item_t` to query.
 * @return The count of `buffer_t` instances associated with the item.
 */
size_t spool_get_buffer_count(const spool_item_t* item);

/**
 * @brief Calculates the total size of all buffers associated with a spool item.
 *
 * This function sums the sizes of all `buffer_t` instances linked to the given spool item.
 *
 * @param item Pointer to the `spool_item_t` to query.
 * @return The combined size in bytes of all buffers for the item.
 */
size_t spool_get_total_buffer_size(const spool_item_t* item);

/**
 * @brief Retrieves a specific buffer from a spool item by its index.
 *
 * @param item Pointer to the `spool_item_t` containing the buffer.
 * @param buf_idx The zero-based index of the desired buffer.
 * @return A constant pointer to the `buffer_t` instance at the specified index, or NULL if the index is out of bounds.
 */
const buffer_t* spool_get_buffer(const spool_item_t* item, size_t buf_idx);

#ifdef __cplusplus
}
#endif

#endif
