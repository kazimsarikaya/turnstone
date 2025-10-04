/**
 * @file wnd_gfx.h
 * @brief window manager graphics header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_GFX_H
#define ___WND_GFX_H

#include <windowmanager/wnd_types.h>
#include <graphics/softgfx.h>

#ifdef __cplusplus
extern "C" {
#endif

void windowmanager_draw_window(windowmanager_t* wndmgr, window_t* window);

int8_t wndmgr_mouse_init(windowmanager_t* wndmgr);
int8_t wndmgr_font_init(windowmanager_t* wndmgr);
void   wndmgr_mouse_move_cursor(windowmanager_t* wndmgr, uint32_t x, uint32_t y);

#ifdef __cplusplus
}
#endif

#endif // ___WND_GFX_H
