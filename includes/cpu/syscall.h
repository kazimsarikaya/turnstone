/**
 * @file syscall.h
 * @brief syscall interface for turnstone 64bit kernel.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#ifndef ___CPU_SYSCALL_H
/*! prevent duplicate header error macro */
#define ___CPU_SYSCALL_H 0

#include <types.h>

void syscall_handler(void);

#endif /* !___CPU_SYSCALL_H */
