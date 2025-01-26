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

rect_t    windowmanager_calc_text_rect(const char_t* text, uint32_t max_width);
uint32_t  windowmanager_append_char16_to_buffer(char16_t src, char_t* dst, uint32_t dst_idx);
boolean_t windowmanager_is_point_in_rect(const rect_t* rect, uint32_t x, uint32_t y);
boolean_t windowmanager_is_rect_in_rect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_is_rects_intersect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_find_window_by_point(window_t* window, uint32_t x, uint32_t y, window_t** result);
boolean_t windowmanager_find_window_by_text_cursor(window_t* window, window_t** result);
int8_t    windowmanager_set_window_text(window_t* window, const char_t* text);
list_t*   windowmanager_get_input_values(const window_t* window);
void      windowmanager_move_cursor_to_next_input(window_t* window, boolean_t is_reverse);
int8_t    windowmanager_destroy_inputs(list_t* inputs);


#ifdef __cplusplus
}
#endif

#endif // ___WND_UTILS_H
