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

typedef void * buffer_t;

typedef enum buffer_seek_direction_t {
    BUFFER_SEEK_DIRECTION_START,
    BUFFER_SEEK_DIRECTION_CURRENT,
    BUFFER_SEEK_DIRECTION_END,
} buffer_seek_direction_t;

buffer_t buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity);
#define buffer_new() buffer_new_with_capacity(NULL, 128)

buffer_t  buffer_append_byte(buffer_t buffer, uint8_t data);
uint64_t  buffer_get_length(buffer_t buffer);
boolean_t buffer_reset(buffer_t buffer);
uint64_t  buffer_get_capacity(buffer_t buffer);
uint64_t  buffer_get_position(buffer_t buffer);
buffer_t  buffer_append_bytes(buffer_t buffer, uint8_t* data, uint64_t length);
buffer_t  buffer_append_buffer(buffer_t buffer, buffer_t appenden);
uint8_t*  buffer_get_bytes(buffer_t buffer, uint64_t length);
uint8_t   buffer_get_byte(buffer_t buffer);
uint8_t*  buffer_get_all_bytes(buffer_t buffer, uint64_t* length);
uint8_t*  buffer_get_all_bytes_and_reset(buffer_t buffer, uint64_t* length);
boolean_t buffer_seek(buffer_t buffer, int64_t position, buffer_seek_direction_t direction);
int8_t    buffer_destroy(buffer_t buffer);
buffer_t  buffer_encapsulate(uint8_t* data, uint64_t length);
uint64_t  buffer_remaining(buffer_t buffer);
uint8_t   buffer_peek_byte_at_position(buffer_t buffer, uint64_t position);
uint8_t   buffer_peek_byte(buffer_t buffer);
uint64_t  buffer_peek_ints_at_position(buffer_t buffer, uint64_t position, uint8_t bc);
uint64_t  buffer_peek_ints(buffer_t buffer, uint8_t bc);
boolean_t buffer_set_readonly(buffer_t buffer, boolean_t ro);
boolean_t buffer_write_slice_into(buffer_t buffer, uint64_t pos, uint64_t len, uint8_t* dest);
#define buffer_write_all_into(b, d) buffer_write_slice_into(b, 0, buffer_get_length(b), d)
uint8_t* buffer_get_view_at_position(buffer_t buffer, uint64_t position, uint64_t length);
#endif
