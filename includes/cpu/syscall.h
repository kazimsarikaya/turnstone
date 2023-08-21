/**
 * @file syscall.h
 * @brief syscall interface for turnstone 64bit kernel.
 **/

#ifndef ___CPU_SYSCALL_H
/*! prevent duplicate header error macro */
#define ___CPU_SYSCALL_H 0

#include <types.h>

void syscall_handler(void);

#endif /* !___CPU_SYSCALL_H */
