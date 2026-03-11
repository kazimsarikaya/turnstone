/**
 * @file wnd_utils.h
 * @brief window manager utilities header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_UTILS_H
#define ___WND_UTILS_H

#include <windowmanager/wnd_types.h>

#ifdef __cplusplus
extern "C" {
#endif

void wndmgr_mouse_move_cursor(windowmanager_t* wndmgr, uint32_t x, uint32_t y);
void wndmgr_text_cursor_move(int32_t x, int32_t y);
void wndmgr_text_cursor_move_relative(int32_t dx, int32_t dy);
void wndmgr_move_cursor_to_next_input(window_t* window, boolean_t is_reverse);

boolean_t wndmgr_find_window_by_text_cursor(window_t* window, const window_t** result);
boolean_t wndmgr_find_window_sheet_by_text_cursor(window_t* window, const window_sheet_t** result);

uint32_t wndmgr_append_char16_to_buffer(char16_t src, char_t* dst, uint32_t dst_idx);

int8_t wndmgr_set_window_text(const window_t* window, const char_t* text);

void      wndmgr_mark_all_windows_dirty(window_t* window);
boolean_t wndmgr_is_window_dirty(const window_t* window);

void wndmgr_make_window_single_sheet(window_t* window);
void wndmgr_mark_window_sheets_always_redrawn(window_t* window, boolean_t is_always_redrawn);

int8_t wndmgr_destroy_inputs(list_t* inputs);

rect_t wndmgr_calc_text_rect(const char_t* text, uint32_t max_width);

boolean_t wndmgr_is_drawing_occured(const window_t* window);

void wndmgr_set_window_writable(const window_t* window, boolean_t is_writable);

list_t* wndmgr_substract_sheets(list_t* sheets, const window_sheet_t* sheet);

list_t* wndmgr_get_input_values(const window_t* window);

void windowmanager_scroll(window_t* window, window_event_t* event);
void windowmanager_enter(window_t* window, window_event_t* event);

#ifdef __cplusplus
}
#endif

#endif // ___WND_UTILS_H
