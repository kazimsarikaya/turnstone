/**
 * @file windowmanager.h
 * @brief window manager header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___WINDOWMANAGER_H
#define ___WINDOWMANAGER_H

#include <types.h>
#include <list.h>
#include <graphics/color.h>
#include <graphics/font.h>


typedef struct window_t window_t;
typedef struct rect_t   rect_t;

struct rect_t {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

typedef int8_t (*window_event_f)(const window_t* window);

struct window_t {
    uint64_t       id;
    char_t*        text;
    boolean_t      is_text_readonly;
    boolean_t      is_dirty;
    boolean_t      is_visible;
    boolean_t      is_writable;
    int32_t        input_length;
    const char_t*  input_id;
    void*          extra_data;
    boolean_t      extra_data_is_allocated;
    int32_t        tab_index;
    rect_t         rect;
    pixel_t*       buffer;
    color_t        background_color;
    color_t        foreground_color;
    window_t*      next;
    window_t*      prev;
    list_t*        children;
    window_event_f on_enter;
    window_event_f on_redraw;
};

typedef struct window_input_value_t {
    const char_t* id;
    char_t*       value;
    void*         extra_data;
    rect_t        rect;
} window_input_value_t;

color_t gfx_blend_colors(color_t color1, color_t color2);
int8_t  gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, color_t color);

void      windowmanager_print_glyph(const window_t* window, uint32_t x, uint32_t y, wchar_t wc);
void      windowmanager_print_text(const window_t* window, uint32_t x, uint32_t y, const char_t* text);
void      windowmanager_clear_screen(window_t* window);
rect_t    windowmanager_calc_text_rect(const char_t* text, uint32_t max_width);
boolean_t windowmanager_draw_window(window_t* window);
uint32_t  windowmanager_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx);
boolean_t windowmanager_is_point_in_rect(const rect_t* rect, uint32_t x, uint32_t y);
boolean_t windowmanager_is_rect_in_rect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_is_rects_intersect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_find_window_by_point(window_t* window, uint32_t x, uint32_t y, window_t** result);
boolean_t windowmanager_find_window_by_text_cursor(window_t* window, window_t** result);
int8_t    windowmanager_set_window_text(window_t* window, const char_t* text);
list_t*   windowmanager_get_input_values(const window_t* window);
void      windowmanager_move_cursor_to_next_input(window_t* window);
int8_t    windowmanager_destroy_inputs(list_t* inputs);

void      windowmanager_destroy_window(window_t* window);
void      windowmanager_insert_and_set_current_window(window_t* window);
void      windowmanager_remove_and_set_current_window(window_t* window);
window_t* windowmanager_create_top_window(void);
window_t* windowmanager_create_window(window_t* parent, char_t* text, rect_t rect, color_t background_color, color_t foreground_color);

window_t* windowmanager_add_option_window(window_t* parent, rect_t pos);

window_t* windowmanager_create_greater_window(void);
window_t* windowmanager_create_primary_options_window(void);
int8_t    windowmanager_create_and_show_spool_browser_window(void);

int8_t    windowmanager_init(void);
boolean_t windowmanager_is_initialized(void);
#endif
