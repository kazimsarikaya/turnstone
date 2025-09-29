/**
 * @file event.h
 * @brief event driver header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___EVENT_H
#define ___EVENT_H

#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

extern buffer_t* kbd_buffer;
extern buffer_t* mouse_buffer;

#ifdef __cplusplus
}
#endif

#endif // ___EVENT_H
