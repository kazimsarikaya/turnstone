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

int8_t  stdbufs_init_buffers(void);
int64_t printf(const char * format, ...) __attribute__((format(printf, 1, 2)));
int64_t vprintf(const char * format, va_list ap);


#endif // ___STDBUFS_H
