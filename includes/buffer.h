/**
 * @file buffer.h
 * @brief buffer interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___BUFFER_H
/*! macro for avoiding multiple inclusion */
 #define ___BUFFER_H 0

#include <types.h>
#include <memory.h>

/**
 * @typedef buffer_t
 * @brief opaque buffer_t struct
 */
typedef struct buffer_t buffer_t; ///< opaque buffer_t struct

/**
 * @enum buffer_seek_direction_t
 * @brief buffer_seek_direction_t is a enum that buffer seek direction
 */
typedef enum buffer_seek_direction_t {
    BUFFER_SEEK_DIRECTION_START, ///< seek from start
    BUFFER_SEEK_DIRECTION_CURRENT, ///< seek from current position
    BUFFER_SEEK_DIRECTION_END, ///< seek from end
} buffer_seek_direction_t; ///< buffer_seek_direction_t is a enum that buffer seek direction

/**
 * @fn buffer_t* buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity)
 * @brief buffer_new_with_capacity creates a new buffer with capacity
 * @param heap heap to allocate buffer
 * @param capacity capacity of buffer
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_new_with_capacity(memory_heap_t* heap, uint64_t capacity);

/**
 * @fn buffer_t* buffer_new(void)
 * @brief buffer_new creates a new buffer with default heap
 * @return buffer_t* pointer to buffer
 */
#define buffer_new() buffer_new_with_capacity(NULL, 128)

/**
 * @fn buffer_t* buffer_create_with_capacity(uint64_t capacity)
 * @brief buffer_create_with_capacity creates a new buffer with capacity
 * @param capacity capacity of buffer
 * @return buffer_t* pointer to buffer
 */
#define buffer_create_with_heap(h, c) buffer_new_with_capacity(h, c)

/**
 * @fn memory_heap_t* buffer_get_heap(buffer_t* buffer)
 * @brief buffer_get_heap returns heap of buffer
 * @param[in] buffer buffer to get heap
 * @return memory_heap_t* pointer to heap
 */
memory_heap_t* buffer_get_heap(buffer_t* buffer);

/**
 * @brief buffer_append_byte appends a byte to buffer
 * @param[in] buffer buffer to append
 * @param[in] data byte to append
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_append_byte(buffer_t* buffer, uint8_t data);

/**
 * @brief returns length of buffer
 * @param[in] buffer buffer to get length
 * @return uint64_t length of buffer
 */
uint64_t buffer_get_length(const buffer_t* buffer);

/**
 * @brief resets buffer, sets length and position to 0 and all bytes to 0, capacity stays same
 * @param[in] buffer buffer to reset
 * @return boolean_t true if success
 */
boolean_t buffer_reset(buffer_t* buffer);

/**
 * @brief returns capacity of buffer
 * @param[in] buffer buffer to get capacity
 * @return uint64_t capacity of buffer
 */
uint64_t buffer_get_capacity(buffer_t* buffer);

/**
 * @brief returns position of buffer
 * @param[in] buffer buffer to get position
 * @return uint64_t position of buffer
 */
uint64_t buffer_get_position(buffer_t* buffer);

/**
 * @brief appends bytes to buffer
 * @param[in] buffer buffer to append
 * @param[in] data bytes to append
 * @param[in] length length of bytes
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_append_bytes(buffer_t* buffer, uint8_t* data, uint64_t length);

/**
 * @brief appends buffer to buffer
 * @param[in] buffer buffer to append
 * @param[in] appenden buffer to append
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_append_buffer(buffer_t* buffer, buffer_t* appenden);

/**
 * @brief returns a byte array from buffer with length, byte array is copied, position is advanced with length
 * @param[in] buffer buffer to get bytes
 * @param[in] length length of bytes
 * @return uint8_t* pointer to byte array
 */
uint8_t* buffer_get_bytes(buffer_t* buffer, uint64_t length);

/**
 * @brief returns a byte array from buffer with length, byte array is copied, position is advanced 1
 * @param[in] buffer buffer to get bytes
 * @return a single byte
 */
uint8_t buffer_get_byte(buffer_t* buffer);

/**
 * @brief returns a byte array containing all bytes from buffer, byte array is copied, position is not advanced
 * @param[in] buffer buffer to get bytes
 * @param[out] length length of bytes returned
 * @return uint8_t* pointer to byte array
 */
uint8_t* buffer_get_all_bytes(buffer_t* buffer, uint64_t* length);

/**
 * @brief returns a byte array containing all bytes from buffer, byte array is copied, position is not advanced, buffer is resetted
 * @param[in] buffer buffer to get bytes
 * @param[out] length length of bytes returned
 * @return uint8_t* pointer to byte array
 */
uint8_t* buffer_get_all_bytes_and_reset(buffer_t* buffer, uint64_t* length);

/**
 * @brief returns a byte array containing all bytes from buffer, byte array is copied, position is not advanced, buffer is destroyed
 * @param[in] buffer buffer to get bytes
 * @param[out] length length of bytes returned
 * @return uint8_t* pointer to byte array
 */
uint8_t* buffer_get_all_bytes_and_destroy(buffer_t* buffer, uint64_t* length);

/**
 * @brief changes position of buffer
 * @param[in] buffer buffer to change position
 * @param[in] position position to change
 * @param[in] direction direction to change
 * @return boolean_t true if success
 */
boolean_t buffer_seek(buffer_t* buffer, int64_t position, buffer_seek_direction_t direction);

/**
 * @brief destroys buffer
 * @param[in] buffer buffer to destroy
 * @return 0 if success
 */
int8_t buffer_destroy(buffer_t* buffer);

/**
 * @brief returns a buffer encapsulating a byte array, byte array is not copied
 * @param[in] data byte array to encapsulate
 * @param[in] length length of byte array
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_encapsulate(uint8_t* data, uint64_t length);

/**
 * @brief returns how many bytes are remaining in buffer (length - position)
 * @param[in] buffer buffer to get remaining bytes
 * @return uint64_t remaining bytes
 */
uint64_t buffer_remaining(buffer_t* buffer);

/**
 * @brief returns a byte from buffer at position, position is not changed
 * @param[in] buffer buffer to peek
 * @param[in] position position to peek
 * @return uint8_t byte at position
 */
uint8_t buffer_peek_byte_at_position(buffer_t* buffer, uint64_t position);

/**
 * @brief returns a byte from buffer at current position, position is not changed
 * @param[in] buffer buffer to peek
 * @return uint8_t byte at position
 */
uint8_t buffer_peek_byte(buffer_t* buffer);

/**
 * @brief returns an int from buffer at position, position is not changed
 * @param[in] buffer buffer to peek
 * @param[in] position position to peek
 * @param[in] bc byte count of int
 * @return uint64_t int at position
 */
uint64_t buffer_peek_ints_at_position(buffer_t* buffer, uint64_t position, uint8_t bc);

/**
 * @brief returns an int from buffer at current position, position is not changed
 * @param[in] buffer buffer to peek
 * @param[in] bc byte count of int
 * @return uint64_t int at position
 */
uint64_t buffer_peek_ints(buffer_t* buffer, uint8_t bc);

/**
 * @brief sets readonly flag of buffer
 * @param[in] buffer buffer to set readonly
 * @param[in] ro readonly flag
 * @return boolean_t true if success
 */
boolean_t buffer_set_readonly(buffer_t* buffer, boolean_t ro);

/**
 * @brief copies bytes from buffer to dest (should be at least length long), position is not changed
 * @param[in] buffer buffer to copy
 * @param[in] pos position to copy
 * @param[in] len length to copy
 * @param[out] dest destination to copy to, dest should be at least len long
 * @return boolean_t true if success
 */
boolean_t buffer_write_slice_into(buffer_t* buffer, uint64_t pos, uint64_t len, uint8_t* dest);

/*! macro to write all bytes from buffer into dest @see buffer_write_slice_into */
#define buffer_write_all_into(b, d) buffer_write_slice_into(b, 0, buffer_get_length(b), d)

/**
 * @brief returns a byte array from buffer with length, byte array is not copied, position is not changed
 * @param[in] buffer buffer to get bytes
 * @param[in] position position to get bytes
 * @param[in] length length of bytes
 * @return uint8_t* pointer to byte array, or null if position + length > buffer length
 */
uint8_t* buffer_get_view_at_position(buffer_t* buffer, uint64_t position, uint64_t length);

/*! default io buffer id for stdin */
#define BUFFER_IO_INPUT 0
/*! default io buffer id for stdout */
#define BUFFER_IO_OUTPUT 1
/*! default io buffer id for stderr */
#define BUFFER_IO_ERROR 2

/**
 * @brief returns a buffer for io buffer id, first three buffers are stdin, stdout, stderr
 * @param[in] buffer_io_id io buffer id
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_get_io_buffer(uint64_t buffer_io_id);

/**
 * @brief prints to buffer, returns number of bytes written
 * @param[in] buffer buffer to print to
 * @param[in] format format string
 * @param[in] ... arguments
 * @return int64_t number of bytes written
 */
int64_t buffer_printf(buffer_t* buffer, const char* format, ...) __attribute__((format(printf, 2, 3)));

/**
 * @brief prints to buffer, returns number of bytes written
 * @param[in] buffer buffer to print to
 * @param[in] format format string
 * @param[in] args va_list of arguments
 * @return int64_t number of bytes written
 */
int64_t buffer_vprintf(buffer_t* buffer, const char* format, va_list args);

/**
 * @brief returns a temporary buffer for printf, before gui is initialized, this is a static buffer, after gui is initialized, this is a buffer allocated with malloc
 * @return buffer_t* pointer to buffer
 */
buffer_t* buffer_get_tmp_buffer_for_printf(void);

/**
 * @brief resets temporary buffer for printf
 */
void buffer_reset_tmp_buffer_for_printf(void);

/**
 * @brief reads a line from buffer, line is null terminated, position is advanced
 * @param[in] buffer buffer to read line
 * @param[in] line_continuation_char line continuation char
 * @param[in] delimiter_char delimiter char
 * @param[out] length length of line optional parameter
 * @return char_t* pointer to line
 */
char_t* buffer_read_line_ext(buffer_t* buffer, char_t line_continuation_char, char_t delimiter_char, uint64_t* length);

/*! macro to read a line from buffer, line is null terminated, position is advanced */
#define buffer_read_line(b) buffer_read_line_ext(b, '\\', '\n', NULL)

uint64_t  buffer_get_mark_position(buffer_t* buffer);
boolean_t buffer_set_mark_position(buffer_t* buffer, uint64_t position);
#endif
