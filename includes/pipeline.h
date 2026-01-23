/**
 * @file pipeline.h
 * @brief Pipeline stream header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___PIPELINE_H
#define ___PIPELINE_H

#include <types.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! @typedef pipeline_t
 * @brief Opaque structure representing a pipeline stream instance.
 */
typedef struct pipeline_t pipeline_t;

/**
 * @brief Creates and initializes a new pipeline stream with a specified heap.
 *
 * This function allocates and initializes a new pipeline_t instance.
 * If `heap` is NULL, the pipeline will use the default system heap for allocations.
 *
 * @param heap Pointer to the memory_heap_t instance to use for allocations, or NULL for default.
 * @param capacity The maximum number of bytes the pipeline can hold.
 * @return A pointer to the newly created pipeline_t instance on success, or NULL on failure.
 */
pipeline_t* pipeline_create_with_heap(memory_heap_t* heap, uint64_t capacity);

/**
 * @brief Creates and initializes a new pipeline stream using the default system heap.
 *
 * This macro is a convenience wrapper around `pipeline_create_with_heap` that
 * automatically uses the default system heap.
 *
 * @param capacity The maximum number of bytes the pipeline can hold.
 * @return A pointer to the newly created pipeline_t instance on success, or NULL on failure.
 */
#define pipeline_create(capacity) pipeline_create_with_heap(NULL, capacity)

/**
 * @brief Deallocates a pipeline stream instance and its internal memory.
 *
 * This function releases all resources associated with the given pipeline_t instance.
 *
 * @param pipeline Pointer to the pipeline_t instance to destroy.
 * @return 0 on success, non-zero on error.
 */
int8_t pipeline_destroy(pipeline_t* pipeline);

/**
 * @brief Writes data to the pipeline.
 *
 * This function attempts to write `size` bytes from `data` into the pipeline.
 * It will write as much data as possible without blocking, up to the available
 * space in the pipeline.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @param size The number of bytes to write.
 * @param data Pointer to the data buffer to write from.
 * @return The actual number of bytes written.
 */
uint64_t pipeline_write(pipeline_t* pipeline, uint64_t size, const uint8_t* data);

/**
 * @brief Writes data to the pipeline, blocking until all data is written.
 *
 * This function attempts to write `size` bytes from `data` into the pipeline.
 * If the pipeline does not have enough space, the function will block until
 * sufficient space becomes available and all data is written.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @param size The number of bytes to write.
 * @param data Pointer to the data buffer to write from.
 * @return The actual number of bytes written (should be `size` on success).
 */
uint64_t pipeline_write_blocked(pipeline_t* pipeline, uint64_t size, const uint8_t* data);

/**
 * @brief Reads data from the pipeline.
 *
 * This function attempts to read `size` bytes from the pipeline into `data`.
 * It will read as much data as possible without blocking, up to the available
 * data in the pipeline.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @param size The maximum number of bytes to read.
 * @param data Pointer to the buffer to store the read data.
 * @return The actual number of bytes read.
 */
uint64_t pipeline_read(pipeline_t* pipeline, uint64_t size, uint8_t* data);

/**
 * @brief Reads data from the pipeline, blocking until all data is read.
 *
 * This function attempts to read `size` bytes from the pipeline into `data`.
 * If the pipeline does not have enough data, the function will block until
 * sufficient data becomes available and all data is read.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @param size The number of bytes to read.
 * @param data Pointer to the buffer to store the read data.
 * @return The actual number of bytes read (should be `size` on success).
 */
uint64_t pipeline_read_blocked(pipeline_t* pipeline, uint64_t size, uint8_t* data);

/**
 * @brief Gets the number of bytes currently available for reading in the pipeline.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @return The number of bytes that can be read from the pipeline.
 */
uint64_t pipeline_available_data(pipeline_t* pipeline);

/**
 * @brief Gets the number of bytes currently available for writing in the pipeline.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @return The number of bytes that can be written to the pipeline.
 */
uint64_t pipeline_available_space(pipeline_t* pipeline);

/**
 * @brief Gets the total capacity of the pipeline.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @return The maximum number of bytes the pipeline can hold.
 */
uint64_t pipeline_capacity(pipeline_t* pipeline);

/**
 * @brief Clears all data from the pipeline.
 *
 * This function resets the pipeline, effectively discarding all currently
 * stored data.
 *
 * @param pipeline Pointer to the pipeline_t instance.
 * @return 0 on success, non-zero on error.
 */
int8_t pipeline_clear(pipeline_t* pipeline);

#ifdef __cplusplus
}
#endif

#endif
