/**
 * @file wnd_types.h
 * @brief window manager types header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_TYPES_H
#define ___WND_TYPES_H

#include <windowmanager.h>
#include <list.h>
#include <graphics/color.h>
#include <graphics/font.h>
#include <graphics/softgfx.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct window_t             window_t;
typedef struct rect_t               rect_t;
typedef struct window_event_t       window_event_t;
typedef struct window_input_value_t window_input_value_t;

typedef enum window_event_type_t {
    WINDOW_EVENT_TYPE_ENTER,
    WINDOW_EVENT_TYPE_REDRAW,
    WINDOW_EVENT_TYPE_SCROLL_UP,
    WINDOW_EVENT_TYPE_SCROLL_DOWN,
    WINDOW_EVENT_TYPE_SCROLL_LEFT,
    WINDOW_EVENT_TYPE_SCROLL_RIGHT,
} window_event_type_t;

typedef struct wndmgr_font_uv_t {
    float32_t u0, v0, u1, v1;
} wndmgr_font_uv_t;

struct windowmanager_t {
    sgfx_context_t*   gfx_ctx;
    sgfx_texture_t    font_texture;
    sgfx_texture_t    mouse_texture;
    uint32_t          screen_width;
    uint32_t          screen_height;
    boolean_t         font_is_sdf;
    uint32_t          font_real_width;
    uint32_t          font_real_height;
    uint32_t          font_width;
    uint32_t          font_height;
    uint32_t          font_column_count;
    uint32_t          font_row_count;
    wndmgr_font_uv_t* font_uv_table;
    uint32_t          padding;
    boolean_t         mouse_initialized;
    uint32_t          mouse_image_width;
    uint32_t          mouse_image_height;
    uint32_t          mouse_x;
    uint32_t          mouse_y;
    window_t*         current_window;
    uint64_t          next_window_id;
    uint64_t          previous_render_time;
};

struct rect_t {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

typedef struct window_event_t {
    window_event_type_t type;
    window_t*           window;
} window_event_t;

typedef int8_t (*window_event_f)(const window_event_t* event);

struct window_t {
    uint64_t       id;
    char_t*        text;
    boolean_t      is_text_readonly;
    boolean_t      is_dirty;
    boolean_t      is_always_redrawn;
    boolean_t      is_drawing_occured;
    boolean_t      is_visible;
    boolean_t      is_writable;
    boolean_t      has_alert;
    int32_t        input_length;
    const char_t*  input_id;
    void*          extra_data;
    boolean_t      extra_data_is_allocated;
    int32_t        tab_index;
    rect_t         rect;
    rect_t         absolute_rect;
    color_t*       buffer;
    color_t        background_color;
    color_t        foreground_color;
    window_t*      parent;
    window_t*      next;
    window_t*      prev;
    list_t*        children;
    window_event_f on_enter;
    window_event_f on_redraw;
    window_event_f on_scroll;
};

typedef struct window_input_value_t {
    const char_t* id;
    char_t*       value;
    void*         extra_data;
    rect_t        rect;
} window_input_value_t;

#define WINDOWMANAGER_COMMAND_TEXT "Command"
#define WINDOWMANAGER_COMMAND_INPUT_TEXT "________________"


#ifdef __cplusplus
}
#endif

#endif
