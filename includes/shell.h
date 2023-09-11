/**
 * @file shell.h
 * @brief shell
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __SHELL_H
#define __SHELL_H 0

#include <types.h>
#include <buffer.h>


extern buffer_t shell_buffer;

int8_t shell_init(void);

#endif /* __SHELL_H */
