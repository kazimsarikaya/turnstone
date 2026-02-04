/**
 * @file wndmgr_utils.64.c
 * @brief Window manager utilities implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_utils.h>
#include <utils.h>
#include <strings.h>
#include <buffer.h>
#include <graphics/screen.h>
#include <graphics/text_cursor.h>
#include <logging.h>

void video_text_print(const char_t* text);

MODULE("turnstone.windowmanager");

extern color_t* VIDEO_BASE_ADDRESS;

static windowmanager_t* wndmgr_instance = NULL;

windowmanager_t* windowmanager_get_instance(void) {
    if(wndmgr_instance == NULL) {
        wndmgr_instance = memory_malloc(sizeof(windowmanager_t));

        if(wndmgr_instance == NULL) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for windowmanager instance\n");
            return NULL;
        }

        screen_info_t screen_info = screen_get_info();

        PRINTLOG(WINDOWMANAGER, LOG_INFO, "Screen res: %dx%d", screen_info.width, screen_info.height);

        sgfx_context_t* gfx_ctx = sgfx_create_context(screen_info.width, screen_info.height, VIDEO_BASE_ADDRESS);

        if(gfx_ctx == NULL) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create graphics context\n");
            memory_free(wndmgr_instance);
            wndmgr_instance = NULL;
            return NULL;
        }

        wndmgr_instance->gfx_ctx = gfx_ctx;
        wndmgr_instance->screen_width = screen_info.width;
        wndmgr_instance->screen_height = screen_info.height;

        wndmgr_instance->padding = 2;
    }

    return wndmgr_instance;
}

rect_t windowmanager_calc_text_rect(const char_t* text, uint32_t max_width) {
    if(text == NULL) {
        return (rect_t){0};
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    rect_t rect = {0};

    rect.x = 0;
    rect.y = 0;
    rect.width = 0;
    rect.height = 0;

    uint32_t max_calc_width = 0;
    size_t len = strlen(text);

    while(*text) {
        if(*text == '\n') {
            rect.height += font_height;
            max_calc_width = MAX(max_calc_width, rect.width);
            rect.width = 0;
        } else {
            if(rect.width + font_width > max_width) {
                rect.height += font_height;
                max_calc_width = MAX(max_calc_width, rect.width);
                rect.width = 0;
            } else {
                rect.width += font_width;
            }
        }

        text++;
    }

    rect.width = MAX(rect.width, max_calc_width);

    if(len > 0 && rect.height == 0) {
        rect.height = font_height;
    }

    return rect;
}

uint32_t windowmanager_append_char16_to_buffer(char16_t src, char_t* dst, uint32_t dst_idx) {
    if(dst == NULL) {
        return NULL;
    }

    int64_t j = dst_idx;

    if(src >= 0x800) {
        dst[j++] = ((src >> 12) & 0xF) | 0xE0;
        dst[j++] = ((src >> 6) & 0x3F) | 0x80;
        dst[j++] = (src & 0x3F) | 0x80;
    } else if(src >= 0x80) {
        dst[j++] = ((src >> 6) & 0x1F) | 0xC0;
        dst[j++] = (src & 0x3F) | 0x80;
    } else {
        dst[j++] = src & 0x7F;
    }

    return j;
}

boolean_t windowmanager_is_point_in_rect(const rect_t* rect, uint32_t x, uint32_t y) {
    if (!rect) {
        return false;
    }

    return (x >= rect->x) &
           (x <  rect->x + rect->width) &
           (y >= rect->y) &
           (y <  rect->y + rect->height);
}

boolean_t windowmanager_is_rect_in_rect(const rect_t* r1, const rect_t* r2) {
    boolean_t valid = (r1 != NULL) & (r2 != NULL);
    return valid & (
        (r2->x >= r1->x) &
        (r2->y >= r1->y) &
        (r2->x + r2->width <= r1->x + r1->width) &
        (r2->y + r2->height <= r1->y + r1->height)
        );
}

boolean_t windowmanager_is_rects_intersect(const rect_t* r1, const rect_t* r2) {
    boolean_t valid = (r1 != NULL) & (r2 != NULL);
    return valid & (
        valid &
        (r1->x + r1->width >= r2->x) &
        (r2->x + r2->width >= r1->x) &
        (r1->y + r1->height >= r2->y) &
        (r2->y + r2->height >= r1->y)
        );
}

rect_t windowmanager_get_window_absolute_rect(const window_t* window) {
    rect_t rect = {0};

    if(window == NULL) {
        return rect;
    }

    rect = window->rect;

    window_t* p = window->parent;

    while(p != NULL) {
        rect.x += p->rect.x;
        rect.y += p->rect.y;
        p = p->parent;
    }

    return rect;
}

boolean_t windowmanager_find_window_by_point(window_t* window, uint32_t x, uint32_t y, window_t** result) {
    if(window == NULL) {
        return false;
    }

    if(result == NULL) {
        return false;
    }

    if(!windowmanager_is_point_in_rect(&window->absolute_rect, x, y)) {
        return false;
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);

        if(windowmanager_find_window_by_point(child, x, y, result)) {
            return true;
        }
    }

    *result = window;

    return true;
}

void windowmanager_mark_window_dirty_by_rect(window_t* window, const rect_t* rect) {
    if (window == NULL || rect == NULL) {
        return;
    }

    // First check children
    boolean_t fully_contained = false;
    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);
        if (windowmanager_is_rects_intersect(&child->absolute_rect, rect)) {
            windowmanager_mark_window_dirty_by_rect(child, rect);

            if (windowmanager_is_rect_in_rect(&child->absolute_rect, rect)) {
                fully_contained = true;
            }

        }
    }

    // Only mark this window if no child contains/intersects the rect
    if (!fully_contained && windowmanager_is_rects_intersect(&window->absolute_rect, rect)) {
        window->is_dirty = true;
    }
}

boolean_t windowmanager_find_window_by_text_cursor(window_t* window, window_t** result) {
    if(window == NULL) {
        return false;
    }

    if(result == NULL) {
        return false;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    x *= font_width;
    y *= font_height;

    return windowmanager_find_window_by_point(window, x, y, result);
}

int8_t windowmanager_set_window_text(window_t* window, const char_t* text) {
    if(window == NULL) {
        return -1;
    }

    if(text == NULL) {
        return -1;
    }

    if(strlen(text) == 0) {
        return -1;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    int32_t win_x = window->absolute_rect.x / font_width;
    int32_t win_y = window->absolute_rect.y / font_height;
    int32_t win_w = window->absolute_rect.width / font_width;

    int32_t start_idx = (y - win_y) * win_w + (x - win_x);

    if(start_idx < 0 || start_idx >= window->input_length) {
        return -1;
    }

    int32_t text_idx = 0;

    while(text[text_idx]) {
        if(start_idx >= window->input_length) {
            break;
        }

        if(text[text_idx] == '\b') {
            if(start_idx > 0) {
                start_idx--;
                x--;
            }
        } else {
            window->text[start_idx] = text[text_idx];
            start_idx++;
            x++;
        }

        if(start_idx >= win_w) {
            x = win_x;
            y++;
        }

        text_idx++;
    }

    text_cursor_move(x, y);

    window->is_dirty = true;

    return 0;
}

static int8_t wndmgr_iv_list_destroyer(memory_heap_t* heap, void* item){
    window_input_value_t* value = (window_input_value_t*)item;

    if(value == NULL) {
        return -1;
    }

    memory_free_ext(heap, value->value);
    memory_free_ext(heap, value);

    return 0;
}

int8_t windowmanager_destroy_inputs(list_t* inputs) {
    if(inputs == NULL) {
        return -1;
    }

    return list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
list_t* windowmanager_get_input_values(const window_t* window) {
    if(window == NULL) {
        return NULL;
    }

    list_t* values = list_create_stack();

    if(values == NULL) {
        return NULL;
    }

    list_t* ws = list_create_stack();

    if(ws == NULL) {
        list_destroy(values);
        return NULL;
    }

    list_stack_push(ws, window);

    while(list_size(ws)) {
        window_t* w = (window_t*)list_stack_pop(ws);

        if(w->is_writable) {
            window_input_value_t* value = memory_malloc(sizeof(window_input_value_t));

            if(value == NULL) {
                list_destroy_with_type(values, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
                list_destroy(ws);
                return NULL;
            }

            value->id = w->input_id;
            value->value = strdup(w->text);
            value->extra_data = w->extra_data;
            value->rect = w->absolute_rect;

            for(size_t i = 0; i < strlen(value->value); i++) { // TODO: find best way for this
                if(value->value[i] == '_') {
                    value->value[i] = ' ';
                }
            }

            for(size_t i = strlen(value->value); i > 0; i--) { // remove trailing spaces
                if(value->value[i - 1] == ' ') {
                    value->value[i - 1] = '\0';
                } else {
                    break;
                }
            }

            if(value->value == NULL) {
                memory_free(value);
                list_destroy_with_type(values, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
                list_destroy(ws);
                return NULL;
            }

            list_stack_push(values, value);
        }

        if(w->children != NULL) {
            for(size_t i = 0; i < list_size(w->children); i++) {
                list_stack_push(ws, list_get_data_at_position(w->children, i));
            }
        }
    }

    return values;
}
#pragma GCC diagnostic pop

void windowmanager_move_cursor_to_next_input(window_t* window, boolean_t is_reverse) {
    if(!window) {
        return;
    }

    list_t* inputs = windowmanager_get_input_values(window);

    if(!inputs) {
        return;
    }

    if(list_size(inputs) == 0) {
        list_destroy(inputs);
        return;
    }

    int32_t cursor_x, cursor_y;

    text_cursor_get(&cursor_x, &cursor_y);

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    cursor_x *= font_width;
    cursor_y *= font_height;

    boolean_t input_found = false;
    const window_input_value_t* first = list_get_data_at_position(inputs, 0);
    const window_input_value_t* last = list_get_data_at_position(inputs, list_size(inputs) - 1);
    const window_input_value_t* next = NULL;

    int64_t end = list_size(inputs) - 1;
    int64_t start = 0;
    int32_t inc = 1;

    if(is_reverse) {
        start = end;
        end = 0;
        inc = -1;
    }

    for(int64_t i = start;
        is_reverse ? i >= end : i <= end;
        i += inc) {
        window_input_value_t* value = (window_input_value_t*)list_get_data_at_position(inputs, i);

        if(!value) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Invalid input value at position %lli", i);
            break;
        }

        if(!input_found && windowmanager_is_point_in_rect(&value->rect, cursor_x, cursor_y)) {
            input_found = true;
            continue;
        }

        if(input_found) {
            next = value;
            break;
        }
    }

    if(!input_found || !next) {
        if(is_reverse) {
            next = last;
        } else {
            next = first;
        }
    }

    if(!next) {
        list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
        return;
    }

    wndmgr_text_cursor_move(next->rect.x / font_width, next->rect.y / font_height);

    list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
}

void wndmgr_text_cursor_move(int32_t x, int32_t y) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    if(wndmgr == NULL) {
        return;
    }

    window_t* wnd = NULL;
    if(windowmanager_find_window_by_text_cursor(wndmgr->current_window, &wnd)) {
        if(wnd != NULL) {
            wnd->is_dirty = true;
        }
    }

    text_cursor_move(x, y);

    wnd = NULL;

    if(windowmanager_find_window_by_text_cursor(wndmgr->current_window, &wnd)) {
        if(wnd != NULL) {
            wnd->is_dirty = true;
        }
    }
}

void wndmgr_text_cursor_move_relative(int32_t dx, int32_t dy) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    if(wndmgr == NULL) {
        return;
    }

    window_t* wnd = NULL;
    if(windowmanager_find_window_by_text_cursor(wndmgr->current_window, &wnd)) {
        if(wnd != NULL) {
            wnd->is_dirty = true;
        }
    }

    text_cursor_move_relative(dx, dy);

    wnd = NULL;

    if(windowmanager_find_window_by_text_cursor(wndmgr->current_window, &wnd)) {
        if(wnd != NULL) {
            wnd->is_dirty = true;
        }
    }
}
