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
#include <video.h>


typedef struct window_t window_t;
typedef union color_t   color_t;
typedef struct rect_t   rect_t;

union color_t {
    struct {
        uint8_t alpha;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } __attribute__((packed));
    uint32_t color;
};

struct rect_t {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
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
    int32_t        tab_index;
    rect_t         rect;
    pixel_t*       buffer;
    color_t        background_color;
    color_t        foreground_color;
    window_t*      next;
    window_t*      prev;
    list_t*        children;
    window_event_f on_enter;
};

typedef struct window_input_value_t {
    const char_t* id;
    char_t*       value;
} window_input_value_t;


extern int32_t VIDEO_GRAPHICS_WIDTH;
extern int32_t VIDEO_GRAPHICS_HEIGHT;
extern uint32_t* VIDEO_BASE_ADDRESS;
extern int32_t VIDEO_PIXELS_PER_SCANLINE;
extern uint8_t* FONT_ADDRESS;
extern int32_t FONT_WIDTH;
extern int32_t FONT_HEIGHT;
extern int32_t FONT_BYTES_PER_GLYPH;
extern int32_t FONT_CHARS_PER_LINE;
extern int32_t FONT_LINES_ON_SCREEN;
extern uint32_t FONT_MASK;
extern int32_t FONT_BYTES_PERLINE;

color_t gfx_blend_colors(color_t color1, color_t color2);
int8_t  gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, pixel_t color);

void      windowmanager_print_glyph(const window_t* window, int32_t x, int32_t y, wchar_t wc);
void      windowmanager_print_text(const window_t* window, int32_t x, int32_t y, const char_t* text);
void      windowmanager_clear_screen(window_t* window);
rect_t    windowmanager_calc_text_rect(const char_t* text, int32_t max_width);
boolean_t windowmanager_draw_window(window_t* window);
uint32_t  windowmanager_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx);
boolean_t windowmanager_is_point_in_rect(const rect_t* rect, int32_t x, int32_t y);
boolean_t windowmanager_is_rect_in_rect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_is_rects_intersect(const rect_t* rect1, const rect_t* rect2);
boolean_t windowmanager_find_window_by_point(window_t* window, int32_t x, int32_t y, window_t** result);
boolean_t windowmanager_find_window_by_text_cursor(window_t* window, window_t** result);
int8_t    windowmanager_set_window_text(window_t* window, const char_t* text);
list_t*   windowmanager_get_input_values(const window_t* window);

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
