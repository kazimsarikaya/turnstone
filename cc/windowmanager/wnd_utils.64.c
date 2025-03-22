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

static pixel_t* wndmgr_double_buffer = NULL;
extern pixel_t* VIDEO_BASE_ADDRESS;

static void windowmanager_clear_screen_area(uint32_t x, uint32_t y, uint32_t width, uint32_t height, color_t background) {
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t line = 0;
    uint32_t bg = background.color;
    screen_info_t screen_info = screen_get_info();

    for(i = 0; i < height; i++) {
        line = (y + i) * screen_info.pixels_per_scanline + x;

        for(j = 0; j < width; j++) {
            *((pixel_t*)(wndmgr_double_buffer + line)) = bg;
            line++;
        }
    }

    SCREEN_FLUSH(0, 0, x, y, width, height);
}

static void windowmanager_screen_flush(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    UNUSED(scanout);
    UNUSED(offset);

    if(VIDEO_BASE_ADDRESS == NULL) {
        return;
    }
    screen_info_t screen_info = screen_get_info();

    uint32_t sw = screen_info.width;
    uint32_t sh = screen_info.height;

    if(x >= sw || y >= sh) {
        return;
    }

    if(x + width > sw) {
        width = sw - x;
    }

    if(y + height > sh) {
        height = sh - y;
    }

    for(uint32_t i = 0; i < height; i++) {
        uint32_t line = (y + i) * screen_info.pixels_per_scanline + x;

        uint8_t* src = (uint8_t*)(wndmgr_double_buffer + line);
        uint8_t* dst = (uint8_t*)(VIDEO_BASE_ADDRESS + line);

        memory_memcopy(src, dst, width * sizeof(pixel_t));
    }
}

static void windowmanager_text_cursor_draw(int32_t x, int32_t y, int32_t width, int32_t height, boolean_t flush) {
    screen_info_t screen_info = screen_get_info();

    int32_t offs = (y * height * screen_info.pixels_per_scanline) + (x * width);
    uint32_t orig_offs = offs;

    int32_t lx, ly, line;

    for(ly = 0; ly < height; ly++) {
        line = offs;

        for(lx = 0; lx < width; lx++) {

            *((pixel_t*)(wndmgr_double_buffer + line)) = ~(*((pixel_t*)(wndmgr_double_buffer + line)));

            line++;
        }

        offs  += screen_info.pixels_per_scanline;
    }

    if(flush) {
        SCREEN_FLUSH(0, orig_offs * sizeof(pixel_t), x * width, y * height, width, height);
    }
}

int8_t windowmanager_init_double_buffer(void) {
    screen_info_t screen_info = screen_get_info();

    uint64_t size = screen_info.pixels_per_scanline * screen_info.height * sizeof(pixel_t);

    wndmgr_double_buffer = memory_malloc(size);

    if(wndmgr_double_buffer == NULL) {
        return -1;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Double buffer initialized at 0x%p with size 0x%llx", wndmgr_double_buffer, size);
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Screen info: %dx%d, pixels per scanline: %d", screen_info.width, screen_info.height, screen_info.pixels_per_scanline);

    SCREEN_FLUSH = windowmanager_screen_flush;
    SCREEN_CLEAR_AREA = windowmanager_clear_screen_area;
    TEXT_CURSOR_DRAW = windowmanager_text_cursor_draw;

    return 0;
}

pixel_t* windowmanager_get_double_buffer(void) {
    return wndmgr_double_buffer;
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

boolean_t windowmanager_find_window_by_point(window_t* window, uint32_t x, uint32_t y, window_t** result) {
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
    text_cursor_hide();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    int32_t win_x = window->rect.x / font_width;
    int32_t win_y = window->rect.y / font_height;
    int32_t win_w = window->rect.width / font_width;

    int32_t start_idx = (y - win_y) * win_w + (x - win_x);

    if(start_idx < 0 || start_idx >= window->input_length) {
        text_cursor_show();
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
    text_cursor_show();

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

    size_t end = list_size(inputs) - 1;
    size_t start = 0;
    int32_t inc = 1;

    if(is_reverse) {
        start = end;
        end = 0;
        inc = -1;
    }

    for(size_t i = start;
        is_reverse ? i >= end : i <= end;
        i += inc) {
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

    text_cursor_hide();
    text_cursor_move(next->rect.x / font_width, next->rect.y / font_height);
    text_cursor_show();

    list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
}
