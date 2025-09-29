/**
 * @file event.h
 * @brief event driver header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/event.h>

MODULE("turnstone.kernel.hw.event");

buffer_t* kbd_buffer = NULL;
buffer_t* mouse_buffer = NULL;
