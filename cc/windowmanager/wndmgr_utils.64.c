/**
 * @file wndmgr_utils.64.c
 * @brief Window manager utilities implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <utils.h>
#include <strings.h>
#include <video.h>
#include <buffer.h>

void video_text_print(const char_t* text);

MODULE("turnstone.windowmanager");

void windowmanager_print_glyph(const window_t* window, int32_t x, int32_t y, wchar_t wc) {
    uint8_t* glyph = FONT_ADDRESS + (wc * FONT_BYTES_PER_GLYPH);

    int32_t abs_x = window->rect.x + x;
    int32_t abs_y = window->rect.y + y;

    int32_t offs = (abs_y * VIDEO_PIXELS_PER_SCANLINE) + abs_x;

    int32_t tx, ty, line, mask;

    for(ty = 0; ty < FONT_HEIGHT; ty++) {
        line = offs;
        mask = FONT_MASK;

        uint32_t tmp = BYTE_SWAP32(*((uint32_t*)glyph));

        for(tx = 0; tx < FONT_WIDTH; tx++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = tmp & mask ? window->foreground_color.color : window->background_color.color;

            mask >>= 1;
            line++;
        }

        glyph += FONT_BYTES_PERLINE;
        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }
}

void windowmanager_print_text(const window_t* window, int32_t x, int32_t y, const char_t* text) {
    if(window == NULL) {
        return;
    }

    if(text == NULL) {
        return;
    }

    int64_t i = 0;

    while(text[i]) {
        wchar_t wc = video_get_wc(text + i, &i);

        if(wc == '\n') {
            y += FONT_HEIGHT;
            x = 0;
        } else {
            windowmanager_print_glyph(window, x, y, wc);
            x += FONT_WIDTH;
        }

        text++;
    }
}

void windowmanager_clear_screen(window_t* window) {
    video_move_text_cursor(0, 0);
    video_text_cursor_hide();
    for(int32_t i = 0; i < VIDEO_GRAPHICS_HEIGHT; i++) {
        for(int32_t j = 0; j < VIDEO_GRAPHICS_WIDTH; j++) {
            *((pixel_t*)(VIDEO_BASE_ADDRESS + (i * VIDEO_PIXELS_PER_SCANLINE) + j)) = window->background_color.color;
        }
    }
}

rect_t windowmanager_calc_text_rect(const char_t* text, int32_t max_width) {
    if(text == NULL) {
        return (rect_t){0};
    }

    rect_t rect = {0};

    rect.x = 0;
    rect.y = 0;
    rect.width = 0;
    rect.height = 0;

    int32_t max_calc_width = 0;
    size_t len = strlen(text);

    while(*text) {
        if(*text == '\n') {
            rect.height += FONT_HEIGHT;
            max_calc_width = MAX(max_calc_width, rect.width);
            rect.width = 0;
        } else {
            if(rect.width + FONT_WIDTH > max_width) {
                rect.height += FONT_HEIGHT;
                max_calc_width = MAX(max_calc_width, rect.width);
                rect.width = 0;
            } else {
                rect.width += FONT_WIDTH;
            }
        }

        text++;
    }

    rect.width = MAX(rect.width, max_calc_width);

    if(len > 0 && rect.height == 0) {
        rect.height = FONT_HEIGHT;
    }

    return rect;
}

uint32_t windowmanager_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx) {
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

boolean_t windowmanager_is_point_in_rect(const rect_t* rect, int32_t x, int32_t y) {
    if(rect == NULL) {
        return false;
    }

    if(x < rect->x) {
        return false;
    }

    if(x >= rect->x + rect->width) {
        return false;
    }

    if(y < rect->y) {
        return false;
    }

    if(y >= rect->y + rect->height) {
        return false;
    }

    return true;
}

boolean_t windowmanager_is_rect_in_rect(const rect_t* rect1, const rect_t* rect2) {
    if(rect1 == NULL || rect2 == NULL) {
        return false;
    }

    if(rect1->x < rect2->x) {
        return false;
    }

    if(rect1->x + rect1->width > rect2->x + rect2->width) {
        return false;
    }

    if(rect1->y < rect2->y) {
        return false;
    }

    if(rect1->y + rect1->height > rect2->y + rect2->height) {
        return false;
    }

    return true;
}

boolean_t windowmanager_is_rects_intersect(const rect_t* rect1, const rect_t* rect2) {
    if(rect1 == NULL || rect2 == NULL) {
        return false;
    }

    if(rect1->x + rect1->width < rect2->x) {
        return false;
    }

    if(rect1->x > rect2->x + rect2->width) {
        return false;
    }

    if(rect1->y + rect1->height < rect2->y) {
        return false;
    }

    if(rect1->y > rect2->y + rect2->height) {
        return false;
    }

    return true;
}

boolean_t windowmanager_find_window_by_point(window_t* window, int32_t x, int32_t y, window_t** result) {
    if(window == NULL) {
        return false;
    }

    if(result == NULL) {
        return false;
    }

    if(!windowmanager_is_point_in_rect(&window->rect, x, y)) {
        return false;
    }

    if(window->children == NULL) {
        *result = window;
        return true;
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);

        if(windowmanager_find_window_by_point(child, x, y, result)) {
            return true;
        }
    }

    return false;
}

boolean_t windowmanager_find_window_by_text_cursor(window_t* window, window_t** result) {
    if(window == NULL) {
        return false;
    }

    if(result == NULL) {
        return false;
    }

    int32_t x, y;

    video_text_cursor_get(&x, &y);

    x *= FONT_WIDTH;
    y *= FONT_HEIGHT;

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

    video_text_cursor_get(&x, &y);
    video_text_cursor_hide();

    int32_t win_x = window->rect.x / FONT_WIDTH;
    int32_t win_y = window->rect.y / FONT_HEIGHT;
    int32_t win_w = window->rect.width / FONT_WIDTH;

    int32_t start_idx = (y - win_y) * win_w + (x - win_x);

    if(start_idx < 0 || start_idx >= window->input_length) {
        video_text_cursor_show();
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

    video_move_text_cursor(x, y);
    video_text_cursor_show();

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
            value->rect = w->rect;

            for(size_t i = 0; i < strlen(value->value); i++) { // TODO: find best way for this
                if(value->value[i] == '_') {
                    value->value[i] = ' ';
                }
            }

            for(size_t i = strlen(value->value); i > 0; i--) { // remove trailing spaces
                if(value->value[i - 1] == ' ') {
                    value->value[i - 1] = '\0';
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

void windowmanager_move_cursor_to_next_input(window_t* window) {
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

    video_text_cursor_get(&cursor_x, &cursor_y);

    cursor_x *= FONT_WIDTH;
    cursor_y *= FONT_HEIGHT;

    boolean_t input_found = false;
    const window_input_value_t* first = list_get_data_at_position(inputs, 0);
    const window_input_value_t* next = NULL;

    for(size_t i = 0; i < list_size(inputs); i++) {
        window_input_value_t* value = (window_input_value_t*)list_get_data_at_position(inputs, i);

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
        next = first;
    }

    if(!next) {
        list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
        return;
    }

    video_text_cursor_hide();
    video_move_text_cursor(next->rect.x / FONT_WIDTH, next->rect.y / FONT_HEIGHT);
    video_text_cursor_show();

    list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
}
