/**
 * @file buffer.h
 * @brief buffer interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___BUFFER_H
#define ___BUFFER_H 0

#include <types.h>
#include <memory.h>

typedef void* buffer_t;

buffer_t buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity);
#define buffer_new() buffer_new_with_capacity(NULL, 128)

buffer_t buffer_append_byte(buffer_t buffer, uint8_t data);
uint64_t buffer_get_length(buffer_t buffer);
buffer_t buffer_append_bytes(buffer_t buffer, uint8_t* data, uint64_t length);
buffer_t buffer_append_buffer(buffer_t buffer, buffer_t appenden);
uint8_t* buffer_get_bytes(buffer_t buffer, uint64_t* length);
int8_t buffer_destroy(buffer_t buffer);


#endif
