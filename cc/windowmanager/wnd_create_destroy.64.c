/**
 * @file wnd_create_destroy.64.c
 * @brief Window manager create and destroy window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <hashmap.h>
#include <buffer.h>
#include <graphics/screen.h>
#include <graphics/text_cursor.h>
#include <time.h>
#include <strings.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

uint64_t windowmanager_next_window_id = 0;
window_t* windowmanager_current_window = NULL;
hashmap_t* windowmanager_windows = NULL;

extern pixel_t* VIDEO_BASE_ADDRESS;

window_t* windowmanager_create_top_window(void) {
    screen_info_t screen_info = screen_get_info();

    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    window->id     = windowmanager_next_window_id++;
    window->rect.x      = 0;
    window->rect.y      = 0;
    window->rect.width  = screen_info.width;
    window->rect.height = screen_info.height;
    window->buffer = VIDEO_BASE_ADDRESS;
    window->background_color.color = 0x00000000;
    window->foreground_color.color = 0xFFFFFFFF;
    window->is_visible = true;
    window->is_dirty = true;

    return window;
}


window_t* windowmanager_create_window(window_t* parent, char_t* text, rect_t rect, color_t background_color, color_t foreground_color) {
    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    int32_t abs_x = parent->rect.x + rect.x;
    int32_t abs_y = parent->rect.y + rect.y;

    window->id     = windowmanager_next_window_id++;
    window->text  = text;
    window->rect   = (rect_t){abs_x, abs_y, rect.width, rect.height};
    window->buffer = VIDEO_BASE_ADDRESS; // + (abs_y * VIDEO_PIXELS_PER_SCANLINE) + abs_x;
    window->background_color = background_color;
    window->foreground_color = foreground_color;
    window->is_visible = true;
    window->is_dirty = true;

    if(parent->children == NULL) {
        parent->children = list_create_queue();
    }

    list_queue_push(parent->children, window);

    return window;
}

static int8_t wndmgr_footer_time_on_redraw(const window_t* window) {
    if(window == NULL) {
        return -1;
    }

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = sprintf("%02d:%02d:%02d %04d-%02d-%02d",
                               tp.hours, tp.minutes, tp.seconds,
                               tp.year, tp.month, tp.day);

    memory_free(window->text);

    ((window_t*)window)->text = time_str;

    return 0;
}

static int8_t wndmgr_create_footer(window_t* parent) {
    if(parent == NULL) {
        return -1;
    }

    font_table_t* ft = font_get_font_table();

    rect_t rect = {0, parent->rect.height - ft->font_height, parent->rect.width, ft->font_height};

    window_t* footer = windowmanager_create_window(parent, NULL, rect, (color_t){.color = 0xFF282828}, (color_t){.color = 0xFFFFFFFF});

    if(footer == NULL) {
        return -1;
    }

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = sprintf("%02d:%02d:%02d %04d-%02d-%02d",
                               tp.hours, tp.minutes, tp.seconds,
                               tp.year, tp.month, tp.day);

    rect = windowmanager_calc_text_rect(time_str, 2000);

    rect.x = parent->rect.width - rect.width - ft->font_width;

    window_t* time_wnd = windowmanager_create_window(footer, time_str, rect, (color_t){.color = 0xFF282828}, (color_t){.color = 0xFF2288FF});

    if(time_wnd == NULL) {
        return -1;
    }

    time_wnd->on_redraw = wndmgr_footer_time_on_redraw;

    return 0;
}

void windowmanager_insert_and_set_current_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(wndmgr_create_footer(window) != 0) {
        return;
    }

    text_cursor_hide();

    window_t* next = windowmanager_current_window->next;

    window->next = next;

    if(next != NULL) {
        next->prev = window;
    }

    window->prev = windowmanager_current_window;
    windowmanager_current_window->next = window;

    windowmanager_current_window = window;
}

void windowmanager_destroy_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->children != NULL) {
        for (size_t i = 0; i < list_size(window->children); i++) {
            window_t* child = (window_t*)list_get_data_at_position(window->children, i);
            windowmanager_destroy_window(child);
        }

        list_destroy(window->children);
    }

    if(!window->is_text_readonly){
        memory_free(window->text);
    }

    if(window->extra_data_is_allocated && window->extra_data != NULL) {
        memory_free(window->extra_data);
    }

    memory_free(window);
}

void windowmanager_remove_and_set_current_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->prev == NULL) {
        return;
    }

    text_cursor_hide();

    window_t* next = windowmanager_current_window->next;
    window_t* prev = window->prev;

    prev->next = next;

    if(next != NULL) {
        next->prev = prev;
    }

    windowmanager_current_window = prev;
    windowmanager_current_window->is_dirty = true;

    windowmanager_destroy_window(window);
}


