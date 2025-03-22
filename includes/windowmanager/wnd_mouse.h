/**
 * @file wnd_mouse.h
 * @brief window mouse operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_MISC_H
#define ___WND_MISC_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t wndmgr_mouse_init(void);
void   wndmgr_mouse_move_cursor(uint32_t x, uint32_t y);


#ifdef __cplusplus
}
#endif

#endif // ___WND_MISC_H
