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

typedef void (*stdbufs_video_printer)(const char_t* string);

int8_t  stdbufs_init_buffers(stdbufs_video_printer video_printer);
int64_t printf(const char * format, ...) __attribute__((format(printf, 1, 2)));
int64_t vprintf(const char * format, va_list ap);
int64_t stdbufs_flush_buffer(buffer_t* buffer);

#ifdef __cplusplus
}
#endif

#endif // ___STDBUFS_H
