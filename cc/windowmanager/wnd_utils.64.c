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
#include <driver/video.h>

MODULE("turnstone.windowmanager");

static windowmanager_t* wndmgr_instance = NULL;

windowmanager_t* windowmanager_get_instance(void) {
    if(!wndmgr_instance) {
        wndmgr_instance = memory_malloc(sizeof(windowmanager_t));

        if(!wndmgr_instance) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for windowmanager instance");
            return NULL;
        }

        screen_info_t screen_info = screen_get_info();

        PRINTLOG(WINDOWMANAGER, LOG_INFO, "Screen res: %dx%d", screen_info.width, screen_info.height);

        sgfx_context_t* gfx_ctx = sgfx_create_context(screen_info.width, screen_info.height,
                                                      video_get_frame_buffer_base_address());

        if(gfx_ctx == NULL) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create graphics context");
            memory_free(wndmgr_instance);
            wndmgr_instance = NULL;
            return NULL;
        }

        sgfx_enable(gfx_ctx, SGFX_CAP_BLEND);

        wndmgr_instance->gfx_ctx       = gfx_ctx;
        wndmgr_instance->screen_width  = screen_info.width;
        wndmgr_instance->screen_height = screen_info.height;

        wndmgr_instance->padding = 2;
    }

    return wndmgr_instance;
}

rect_t wndmgr_calc_text_rect(const char16_t* text, uint32_t max_width) {
    if(!text) {
        return (rect_t){0};
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;

    rect_t rect = {0};

    rect.x      = 0;
    rect.y      = 0;
    rect.width  = 0;
    rect.height = 0;

    uint32_t max_calc_width = 0;
    size_t len              = wstrlen(text);

    while(*text) {
        if(*text == u'\n') {
            rect.height   += font_height;
            max_calc_width = MAX(max_calc_width, rect.width);
            rect.width     = 0;
        } else {
            if(rect.width + font_width > max_width) {
                rect.height   += font_height;
                max_calc_width = MAX(max_calc_width, rect.width);
                rect.width     = 0;
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

char16_t* wndmgr_crop_text_to_rect(const windowmanager_t* wndmgr, const char16_t* text, rect_t text_rect, rect_t rect) {
    if(!wndmgr || !text) {
        return NULL;
    }

    // 1. Calculate how many characters fit in the sheet
    int32_t sheet_cols = rect.width / wndmgr->font_width;
    int32_t sheet_rows = rect.height / wndmgr->font_height;

    // 2. Calculate the character offset into the source text
    // Offset = (Sheet relative X / font_width)
    int32_t start_col     = (rect.x - text_rect.x) / wndmgr->font_width;
    int32_t start_row     = (rect.y - text_rect.y) / wndmgr->font_height;
    int32_t source_stride = text_rect.width / wndmgr->font_width + 1;

    // 3. Allocate buffer
    // Size: (chars per row + newline) * rows + null terminator
    size_t buf_size  = (sheet_cols + 1) * sheet_rows + 1;
    char16_t* buffer = memory_malloc(buf_size * sizeof(char16_t));
    if(!buffer) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for cropped text buffer");
        return NULL;
    }

    size_t text_len = wstrlen(text);

    uint32_t out_idx = 0;

    for(int32_t i = 0; i < sheet_rows && out_idx < buf_size - 1; i++) {
        boolean_t text_ended = false;
        for(int32_t j = 0; j < sheet_cols && out_idx < buf_size - 1; j++) {
            // Calculate coordinate in the source "2D array"
            int32_t src_y = start_row + i;
            int32_t src_x = start_col + j;

            size_t text_index = (src_y * source_stride) + src_x;

            if(text_index >= text_len) {
                text_ended = true;
                break;
            }

            buffer[out_idx++] = text[text_index];
        }

        if(text_ended) {
            break;
        }

        // dont put new line after last line
        if(i < sheet_rows - 1) {
            buffer[out_idx++] = u'\n';
        }
    }

    buffer[out_idx] = u'\0';

    return buffer;
}

static boolean_t wndmgr_is_point_in_rect(const rect_t* rect, uint32_t x, uint32_t y) {
    return (rect != NULL) &
           (x >= rect->x) &
           (x <  rect->x + rect->width) &
           (y >= rect->y) &
           (y <  rect->y + rect->height);
}

#if 0
static boolean_t wndmgr_is_rect_in_rect(const rect_t* r1, const rect_t* r2) {
    boolean_t valid = (r1 != NULL) & (r2 != NULL);
    return valid & (
        (r2->x >= r1->x) &
        (r2->y >= r1->y) &
        (r2->x + r2->width <= r1->x + r1->width) &
        (r2->y + r2->height <= r1->y + r1->height)
        );
}
#endif

static boolean_t wndmgr_is_rects_intersect(const rect_t* r1, const rect_t* r2) {
    boolean_t valid = (r1 != NULL) & (r2 != NULL);
    return valid & (
        valid &
        (r1->x + r1->width >= r2->x) &
        (r2->x + r2->width >= r1->x) &
        (r1->y + r1->height >= r2->y) &
        (r2->y + r2->height >= r1->y)
        );
}

static boolean_t wndmgr_find_window_by_point(const window_t* window, uint32_t x, uint32_t y, const window_t** result) {
    if(!window || !result) {
        return false;
    }

    // if point not in owner rect of this window, return false
    if(!wndmgr_is_point_in_rect(&window->owner_absolute_rect, x, y)) {
        return false;
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        const window_t* child = list_get_data_at_position(window->children, i);

        if(!wndmgr_is_point_in_rect(&child->owner_absolute_rect, x, y)) {
            continue;
        }

        if(wndmgr_find_window_by_point(child, x, y, result)) {
            return true;
        }
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        const window_sheet_t* sheet = list_get_data_at_position(window->sheets, i);

        if(wndmgr_is_point_in_rect(&sheet->absolute_rect, x, y)) {
            *result = window;
            return true;
        }
    }

    *result = window;

    return false;
}

static boolean_t wndmgr_find_window_sheet_by_point(const window_t* window, uint32_t x, uint32_t y, const window_sheet_t** result) {
    if(!window || !result) {
        return false;
    }

    // if point not in owner rect of this window, return false
    if(!wndmgr_is_point_in_rect(&window->owner_absolute_rect, x, y)) {
        return false;
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        const window_t* child = list_get_data_at_position(window->children, i);

        if(!wndmgr_is_point_in_rect(&child->owner_absolute_rect, x, y)) {
            continue;
        }

        if(wndmgr_find_window_sheet_by_point(child, x, y, result)) {
            return true;
        }
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        const window_sheet_t* sheet = list_get_data_at_position(window->sheets, i);

        if(wndmgr_is_point_in_rect(&sheet->absolute_rect, x, y)) {
            *result = sheet;
            return true;
        }
    }

    return false;
}

static void wndmgr_mark_window_sheet_dirty_by_rect(const window_t* window, const rect_t* rect) {
    if (!window || !rect) {
        return;
    }

    // if rect not intersect with owner rect of this window, return
    if (!wndmgr_is_rects_intersect(&window->owner_absolute_rect, rect)) {
        return;
    }

    // First check children
    for (size_t i = 0; i < list_size(window->children); i++) {
        const window_t* child = list_get_data_at_position(window->children, i);

        wndmgr_mark_window_sheet_dirty_by_rect(child, rect);
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);

        if (wndmgr_is_rects_intersect(&sheet->absolute_rect, rect)) {
            sheet->is_dirty = true;
        }
    }
}

boolean_t wndmgr_find_window_by_text_cursor(const window_t* window, const window_t** result) {
    if(!window || !result) {
        return false;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    x *= window->wndmgr->font_width;
    y *= window->wndmgr->font_height;

    return wndmgr_find_window_by_point(window, x, y, result);
}

boolean_t wndmgr_find_window_sheet_by_text_cursor(const window_t* window, const window_sheet_t** result) {
    if(!window || !result) {
        return false;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    x *= window->wndmgr->font_width;
    y *= window->wndmgr->font_height;

    return wndmgr_find_window_sheet_by_point(window, x, y, result);
}

int8_t wndmgr_set_window_text(const window_t* window, const char16_t* text) {
    if(window == NULL) {
        return -1;
    }

    if(!window->sheets) {
        return -1;
    }

    if(text == NULL) {
        return -1;
    }

    if(wstrlen(text) == 0) {
        return -1;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    uint32_t font_width = window->wndmgr->font_width, font_height = window->wndmgr->font_height;

    const window_sheet_t* sheet = NULL;

    if(!wndmgr_find_window_sheet_by_text_cursor(window, &sheet)) {
        return -1;
    }

    int32_t win_x = sheet->absolute_rect.x / font_width;
    int32_t win_y = sheet->absolute_rect.y / font_height;
    int32_t win_w = sheet->absolute_rect.width / font_width;

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
            sheet->text[start_idx] = text[text_idx];
            start_idx++;
            x++;
        }

        if(start_idx >= win_w) {
            x = win_x;
            y++;
        }

        text_idx++;
    }

    wndmgr_text_cursor_move(x, y);

    ((window_sheet_t*)sheet)->is_dirty = true;

    return 0;
}

static void wndmgr_mark_window_sheet_dirty_by_text_cursor(const window_t* window) {
    if(window == NULL) {
        return;
    }

    int32_t x, y;

    text_cursor_get(&x, &y);

    x *= window->wndmgr->font_width;
    y *= window->wndmgr->font_height;

    rect_t cursor_rect = {x, y, window->wndmgr->font_width, window->wndmgr->font_height};

    wndmgr_mark_window_sheet_dirty_by_rect(window, &cursor_rect);
}

void wndmgr_text_cursor_move(int32_t x, int32_t y) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    if(!wndmgr) {
        return;
    }

    wndmgr_mark_window_sheet_dirty_by_text_cursor(wndmgr->current_window);

    text_cursor_move(x, y);

    wndmgr_mark_window_sheet_dirty_by_text_cursor(wndmgr->current_window);
}

void wndmgr_text_cursor_move_relative(int32_t dx, int32_t dy) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    if(!wndmgr) {
        return;
    }

    wndmgr_mark_window_sheet_dirty_by_text_cursor(wndmgr->current_window);

    text_cursor_move_relative(dx, dy);

    wndmgr_mark_window_sheet_dirty_by_text_cursor(wndmgr->current_window);
}

void wndmgr_mouse_move_cursor(const windowmanager_t* wndmgr, uint32_t x, uint32_t y) {
    if (!wndmgr->mouse_initialized) {
        return;
    }

    if (x >= wndmgr->screen_width || y >= wndmgr->screen_height) {
        video_text_print("Mouse cursor position out of bounds\n");
        return;
    }

    rect_t mouse_rect = {
        .x      = wndmgr->mouse_x,
        .y      = wndmgr->mouse_y,
        .width  = wndmgr->mouse_image_width,
        .height = wndmgr->mouse_image_height
    };

    wndmgr_mark_window_sheet_dirty_by_rect(wndmgr->current_window, &mouse_rect);


    ((windowmanager_t*)wndmgr)->mouse_x = x;
    ((windowmanager_t*)wndmgr)->mouse_y = y;

    mouse_rect.x = wndmgr->mouse_x;
    mouse_rect.y = wndmgr->mouse_y;

    wndmgr_mark_window_sheet_dirty_by_rect(wndmgr->current_window, &mouse_rect);
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

int8_t wndmgr_destroy_inputs(list_t* inputs) {
    if(inputs == NULL) {
        return -1;
    }

    return list_destroy_with_type(inputs, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
list_t* wndmgr_get_input_values(const window_t* window) {
    if(!window) {
        return NULL;
    }

    list_t* values = list_create_stack();

    if(!values) {
        return NULL;
    }

    list_t* ws = list_create_stack();

    if(!ws) {
        list_destroy(values);
        return NULL;
    }

    list_stack_push(ws, window);

    while(list_size(ws)) {
        window_t* w = (window_t*)list_stack_pop(ws);

        if(w->children) {
            for(size_t i = 0; i < list_size(w->children); i++) {
                list_stack_push(ws, list_get_data_at_position(w->children, i));
            }
        }

        if(!w->sheets) {
            continue;
        }

        boolean_t has_writable_sheet = true;
        for(size_t i = 0; i < list_size(w->sheets); i++) {
            const window_sheet_t* sheet = list_get_data_at_position(w->sheets, i);

            if(!sheet->is_writable) {
                has_writable_sheet = false;
                break;
            }
        }

        if(!has_writable_sheet) {
            continue;
        }

        char16_t input_buffer[w->input_length + 1];
        memory_memclean(input_buffer, sizeof(input_buffer));

        for(size_t i = 0; i < list_size(w->sheets); i++) {
            const window_sheet_t* sheet = list_get_data_at_position(w->sheets, i);

            memory_memcopy(sheet->text, input_buffer + wstrlen(input_buffer),
                           MIN(wstrlen(sheet->text), sizeof(input_buffer) - wstrlen(input_buffer) - 1));
        }

        video_text_print("raw input value: ");
        for(int32_t i = 0; i < w->input_length; i++) {
            char_t* blabla = strprintf("%02x ", input_buffer[i]);
            video_text_print(blabla);
            memory_free(blabla);
        }
        video_text_print("\n");

        // remove spaces from the beginning and the end of the input buffer
        size_t start = 0, end = wstrlen(input_buffer);
        while(input_buffer[start] == ' ' && start < end) {
            start++;
        }
        while(end > start && input_buffer[end - 1] == ' ') {
            end--;
        }
        input_buffer[end] = '\0';

        window_input_value_t* value = memory_malloc(sizeof(window_input_value_t));

        if(value == NULL) {
            list_destroy_with_type(values, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
            list_destroy(ws);
            return NULL;
        }

        value->id         = w->input_id;
        value->value      = wstrdup(input_buffer + start);
        value->extra_data = w->extra_data;
        value->rect       = w->owner_absolute_rect;


        if(!value->value) {
            memory_free(value);
            list_destroy_with_type(values, LIST_DESTROY_WITH_DATA, wndmgr_iv_list_destroyer);
            list_destroy(ws);
            return NULL;
        }

        list_stack_push(values, value);
    }

    return values;
}
#pragma GCC diagnostic pop

void wndmgr_move_cursor_to_next_input(const window_t* window, boolean_t is_reverse) {
    if(!window) {
        return;
    }

    list_t* inputs = wndmgr_get_input_values(window);

    if(!inputs) {
        video_text_print("Failed to get input values\n");
        return;
    }

    if(list_size(inputs) == 0) {
        list_destroy(inputs);
        video_text_print("No input values found\n");
        return;
    }

    int32_t cursor_x, cursor_y;

    text_cursor_get(&cursor_x, &cursor_y);

    uint32_t font_width = window->wndmgr->font_width, font_height = window->wndmgr->font_height;

    cursor_x *= font_width;
    cursor_y *= font_height;

    boolean_t input_found             = false;
    const window_input_value_t* first = list_get_data_at_position(inputs, 0);
    const window_input_value_t* last  = list_get_data_at_position(inputs, list_size(inputs) - 1);
    const window_input_value_t* next  = NULL;

    int64_t end   = list_size(inputs) - 1;
    int64_t start = 0;
    int32_t inc   = 1;

    if(is_reverse) {
        start = end;
        end   = 0;
        inc   = -1;
    }

    for(int64_t i = start;
        is_reverse ? i >= end : i <= end;
        i += inc) {
        window_input_value_t* value = (window_input_value_t*)list_get_data_at_position(inputs, i);

        if(!value) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Invalid input value at position %lli", i);
            break;
        }

        if(!input_found && wndmgr_is_point_in_rect(&value->rect, cursor_x, cursor_y)) {
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

boolean_t wndmgr_is_drawing_occured(const window_t* window) {
    if(window == NULL) {
        return false;
    }

    if(!window->sheets) {
        return false;
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        const window_sheet_t* sheet = list_get_data_at_position(window->sheets, i);

        if(sheet->is_drawing_occured) {
            return true;
        }
    }

    return false;
}

void wndmgr_mark_all_windows_dirty(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->sheets) {
        for(size_t i = 0; i < list_size(window->sheets); i++) {
            window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);
            sheet->is_dirty = true;
        }
    }

    if(window->children) {
        for(size_t i = 0; i < list_size(window->children); i++) {
            window_t* child = (window_t*)list_get_data_at_position(window->children, i);
            wndmgr_mark_all_windows_dirty(child);
        }
    }
}

boolean_t wndmgr_is_window_dirty(const window_t* window) {
    if(window == NULL) {
        return false;
    }

    if(!window->sheets) {
        return false;
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        const window_sheet_t* sheet = list_get_data_at_position(window->sheets, i);

        if(sheet->is_dirty) {
            return true;
        }
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        const window_t* child = list_get_data_at_position(window->children, i);

        if(wndmgr_is_window_dirty(child)) {
            return true;
        }
    }

    return false;
}

void wndmgr_set_window_writable(const window_t* window, boolean_t is_writable) {
    if(window == NULL) {
        return;
    }

    if(!window->sheets) {
        return;
    }

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);
        sheet->is_writable = is_writable;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t wndmgr_add_sheet_fragment(list_t * result, window_sheet_t * source, rect_t new_abs_rect) {
    if(new_abs_rect.width <= 0 || new_abs_rect.height <= 0) {
        return 0;
    }

    window_sheet_t* fragment = memory_malloc(sizeof(window_sheet_t));
    if (!fragment) {
        return -1;
    }

    // Copy all properties
    *fragment = *source;

    // Calculate the offset between absolute and relative coordinates
    int32_t offset_x = source->absolute_rect.x - source->rect.x;
    int32_t offset_y = source->absolute_rect.y - source->rect.y;

    // Update both rectangles
    fragment->absolute_rect = new_abs_rect;
    fragment->rect.x        = new_abs_rect.x - offset_x;
    fragment->rect.y        = new_abs_rect.y - offset_y;
    fragment->rect.width    = new_abs_rect.width;
    fragment->rect.height   = new_abs_rect.height;

    if(list_list_insert(result, fragment) == -1ULL) {
        memory_free(fragment);
        return -1;
    }

    return 0;
}

list_t* wndmgr_substract_sheets(list_t* sheets, const window_sheet_t* sheet) {
    if(!sheets || !sheet) {
        return NULL;
    }

    list_t* result = list_create_list();

    if(!result) {
        return NULL;
    }

    for(size_t i = 0; i < list_size(sheets); i++) {
        window_sheet_t* s = (window_sheet_t*)list_get_data_at_position(sheets, i);

        if(!wndmgr_is_rects_intersect(&s->absolute_rect, &sheet->absolute_rect)) {
            window_sheet_t* new_sheet = memory_malloc(sizeof(window_sheet_t));

            if(!new_sheet) {
                goto err;
            }

            *new_sheet = *s;

            if(list_list_insert(result, new_sheet) == -1ULL) {
                memory_free(new_sheet);
                goto err;
            }
        } else {
            // If the sheet intersects, split 's' into up to 4 fragments
            rect_t s_rect = s->absolute_rect;
            rect_t clip   = sheet->absolute_rect;

            // Pre-check: Is 's' completely swallowed by 'sheet'?
            if (s_rect.x >= clip.x &&
                s_rect.y >= clip.y &&
                s_rect.x + s_rect.width <= clip.x + clip.width &&
                s_rect.y + s_rect.height <= clip.y + clip.height) {
                continue; // 's' is entirely covered, discard it
            }

            // 1. Top fragment
            if (clip.y > s_rect.y) {
                rect_t r = {s_rect.x, s_rect.y, s_rect.width, clip.y - s_rect.y};
                if(wndmgr_add_sheet_fragment(result, s, r) != 0) {
                    goto err;
                }
            }

            // 2. Bottom fragment
            if (clip.y + clip.height < s_rect.y + s_rect.height) {
                rect_t r = {s_rect.x, clip.y + clip.height, s_rect.width, (s_rect.y + s_rect.height) - (clip.y + clip.height)};
                if(wndmgr_add_sheet_fragment(result, s, r) != 0) {
                    goto err;
                }
            }

            // 3. Left fragment (between the new top and bottom)
            int32_t clip_top    = (clip.y > s_rect.y) ? clip.y : s_rect.y;
            int32_t clip_bottom = (clip.y + clip.height < s_rect.y + s_rect.height) ? clip.y + clip.height : s_rect.y + s_rect.height;

            if (clip.x > s_rect.x) {
                rect_t r = {s_rect.x, clip_top, clip.x - s_rect.x, clip_bottom - clip_top};
                if(wndmgr_add_sheet_fragment(result, s, r) != 0) {
                    goto err;
                }
            }

            // 4. Right fragment (between the new top and bottom)
            if (clip.x + clip.width < s_rect.x + s_rect.width) {
                rect_t r = {clip.x + clip.width, clip_top, (s_rect.x + s_rect.width) - (clip.x + clip.width), clip_bottom - clip_top};
                if(wndmgr_add_sheet_fragment(result, s, r) != 0) {
                    goto err;
                }
            }

        }
    }

    for(size_t i = 0; i < list_size(sheets); i++) {
        window_sheet_t* s = (window_sheet_t*)list_get_data_at_position(sheets, i);
        memory_free(s);
    }

    list_destroy(sheets);

    return result;
err:
    if(result) {
        for(size_t i = 0; i < list_size(result); i++) {
            window_sheet_t* s = (window_sheet_t*)list_get_data_at_position(result, i);
            memory_free(s);
        }
        list_destroy(result);
    }
    return NULL;
}
#pragma GCC diagnostic pop

void wndmgr_mark_window_sheets_always_redrawn(window_t* window, boolean_t is_always_redrawn) {
    if(!window) {
        return;
    }

    if(window->sheets) {
        for(size_t i = 0; i < list_size(window->sheets); i++) {
            window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);
            sheet->is_always_redrawn = is_always_redrawn;
        }
    }

    // TODO: do we need this?
    if(window->children) {
        for(size_t i = 0; i < list_size(window->children); i++) {
            window_t* child = (window_t*)list_get_data_at_position(window->children, i);
            wndmgr_mark_window_sheets_always_redrawn(child, is_always_redrawn);
        }
    }
}

void windowmanager_scroll(window_t* window, window_event_t* event) {
    if(!window || !event) {
        return;
    }

    if(window->on_scroll) {
        window->on_scroll(event);
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);
        event->window = child;
        windowmanager_scroll(child, event);
    }
}

void windowmanager_enter(window_t* window, window_event_t* event) {
    if(!window || !event) {
        return;
    }

    if(window->on_enter) {
        window->on_enter(event);
    }

    for(size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);
        event->window = child;
        windowmanager_enter(child, event);
    }
}
