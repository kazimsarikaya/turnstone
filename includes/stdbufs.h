/**
 * @file stdbufs.h
 * @brief standard input/output buffers
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___STDBUFS_H
#define ___STDBUFS_H

#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for video printing.
 *
 * This type defines the signature for a function that can print a null-terminated
 * string to a video output device or console.
 *
 * @param string The null-terminated string to be printed.
 */
typedef void (*stdbufs_video_printer_f)(const char_t* string);

/**
 * @brief Sets the video printer function for standard I/O buffers.
 *
 * This function allows the caller to specify a custom video printer function
 * that will be used to output data from the standard output buffer. The provided
 * function should match the `stdbufs_video_printer` signature.
 *
 * @param video_printer A function pointer to the video printing routine.
 *                      This function will be called by `stdbufs_flush_buffer`
 *                      to display buffered output.
 * @return 0 on success, or a non-zero error code on failure.
 */
int8_t stdbufs_set_video_printer(stdbufs_video_printer_f video_printer);

/**
 * @brief Initializes the standard input/output buffers.
 *
 * This function sets up the internal buffers used for standard I/O operations,
 * such as `printf`. It also registers a video printer function that will be
 * used to output data to the display.
 *
 * @param video_printer A function pointer to the video printing routine.
 *                      This function will be called by `stdbufs_flush_buffer`
 *                      to display buffered output.
 * @return 0 on successful initialization, or a non-zero error code on failure.
 */
int8_t stdbufs_init_buffers(stdbufs_video_printer_f video_printer);

/**
 * @brief Prints formatted output to the standard output buffer.
 *
 * This function works similarly to the standard C library `printf`,
 * formatting a string and its arguments and writing the result to an
 * internal standard output buffer. The buffer is flushed to the video
 * printer when it's full or explicitly flushed.
 *
 * @param format A format string, as described in `printf`.
 * @param ... Variable arguments to be formatted according to `format`.
 * @return The number of characters written to the buffer, or a negative value on error.
 */
int64_t printf(const char * format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Prints formatted output to the standard output buffer using a `va_list`.
 *
 * This function is similar to `printf` but takes a `va_list` argument,
 * making it suitable for implementing custom `printf`-like functions.
 *
 * @param format A format string, as described in `printf`.
 * @param ap A `va_list` containing the arguments to be formatted.
 * @return The number of characters written to the buffer, or a negative value on error.
 */
int64_t vprintf(const char * format, va_list ap);

/**
 * @brief Sets the postphone flush behavior for standard I/O buffers.
 *
 * This function allows the caller to specify whether the flushing of the
 * standard output buffer should be postphoned. When `postphone_flush` is set
 * to `true`, the buffer will not be flushed immediately, and the caller will
 * need to call `stdbufs_flush_buffer` explicitly to flush the contents.
 *
 * @param postphone_flush A boolean value indicating whether to postphone flush (true) or not (false).
 */
void stdbufs_set_postphone_flush(boolean_t postphone_flush);

/**
 * @brief Flushes the contents of a specified buffer to its associated output.
 *
 * For the standard output buffer, this function will call the `video_printer`
 * function (provided during `stdbufs_init_buffers`) to display the buffered content.
 *
 * @param buffer Pointer to the `buffer_t` instance to be flushed.
 * @return The number of bytes successfully flushed, or a negative value on error.
 */
int64_t stdbufs_flush_buffer(buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // ___STDBUFS_H
