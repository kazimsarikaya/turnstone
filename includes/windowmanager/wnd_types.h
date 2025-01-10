/**
 * @file wnd_types.h
 * @brief window manager types header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_TYPES_H
#define ___WND_TYPES_H

#include <types.h>
#include <list.h>
#include <graphics/color.h>
#include <graphics/font.h>

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
    window_event_f on_scroll;
};

typedef struct window_input_value_t {
    const char_t* id;
    char_t*       value;
    void*         extra_data;
    rect_t        rect;
} window_input_value_t;


#endif
