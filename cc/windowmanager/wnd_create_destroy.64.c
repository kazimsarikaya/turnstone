/**
 * @file wnd_create_destroy.64.c
 * @brief Window manager create and destroy window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
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
    window->buffer = windowmanager_get_double_buffer();
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
    window->buffer = windowmanager_get_double_buffer();
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

static int8_t wndmgr_footer_time_on_redraw(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = strprintf("%02d:%02d:%02d %04d-%02d-%02d",
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

    char_t* time_str = strprintf("%02d:%02d:%02d %04d-%02d-%02d",
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

void windowmanager_destroy_child_window(window_t* window, window_t* child) {
    if(window == NULL || child == NULL) {
        return;
    }

    if(window->children == NULL) {
        return;
    }

    list_list_delete(window->children, child);

    windowmanager_destroy_window(child);
}

void windowmanager_destroy_all_child_windows(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->children == NULL) {
        return;
    }

    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);
        windowmanager_destroy_window(child);
    }

    list_destroy(window->children);

    window->children = NULL;
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

typedef struct wndmgr_alert_window_data {
    window_event_f on_enter;
    window_t*      self;
    void*          extra_data;
} wndmgr_alert_window_data_t;

static int8_t wndmgr_alert_window_on_enter(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    wndmgr_alert_window_data_t* alert_window_data = (wndmgr_alert_window_data_t*)windowmanager_current_window->extra_data;
    window_t* alert_window = alert_window_data->self;

    if(alert_window == NULL) {
        return -1;
    }

    list_list_delete(windowmanager_current_window->children, alert_window);

    windowmanager_current_window->on_enter = alert_window_data->on_enter;
    windowmanager_current_window->extra_data = alert_window_data->extra_data;
    windowmanager_current_window->has_alert = false;
    windowmanager_current_window->is_dirty = true;

    windowmanager_destroy_window(alert_window);

    return 0;
}

void windowmanager_create_and_show_alert_window(windowmanager_alert_window_type_t type, const char_t* text) {
    screen_info_t screen_info = screen_get_info();
    uint32_t font_width = 0, font_height = 0;
    font_get_font_dimension(&font_width, &font_height);

    rect_t rect = windowmanager_calc_text_rect(text, 400);

    rect_t alert_rect = {0, 0, rect.width + font_width * 4, rect.height + font_height * 4};

    alert_rect.x = (screen_info.width - alert_rect.width) / 2;
    alert_rect.y = (screen_info.height - alert_rect.height) / 2;

    int32_t linecharcount = alert_rect.width / font_width;
    int32_t linecount = alert_rect.height / font_height;

    buffer_t* text_buffer = buffer_new_with_capacity(NULL, linecharcount * linecount);

    if(text_buffer == NULL) {
        return;
    }

    for(int32_t i = 0; i < linecharcount; i++) {
        buffer_append_byte(text_buffer, '*');
    }

    for(int32_t i = 0; i < linecount - 2; i++) {
        buffer_append_bytes(text_buffer, (uint8_t*)"* ", 2);

        for(int32_t j = 0; j < linecharcount - 4; j++) {
            buffer_append_byte(text_buffer, ' ');
        }

        buffer_append_bytes(text_buffer, (uint8_t*)" *", 2);
    }

    for(int32_t i = 0; i < linecharcount; i++) {
        buffer_append_byte(text_buffer, '*');
    }

    char_t* frame_text = (char_t*)buffer_get_all_bytes_and_destroy(text_buffer, NULL);

    color_t foreground_color;
    color_t background_color = {.color = 0x00000000};

    if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_INFO) {
        foreground_color.color = 0xFF2288FF;
    } else if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_WARNING) {
        foreground_color.color = 0xFFFF8800;
    } else if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_ERROR) {
        foreground_color.color = 0xFFFF0000;
    } else {
        foreground_color.color = 0xFFFFFFFF;
    }

    window_t* alert_window = windowmanager_create_window(windowmanager_current_window, frame_text, alert_rect, background_color, foreground_color);

    if(alert_window == NULL) {
        memory_free(frame_text);
        return;
    }

    rect.x = 2 * font_width;
    rect.y = 2 * font_height;

    window_t* text_window = windowmanager_create_window(alert_window, strdup(text), rect, background_color, foreground_color);

    if(text_window == NULL) {
        memory_free(frame_text);
        windowmanager_destroy_window(alert_window);
        return;
    }

    wndmgr_alert_window_data_t* alert_window_data = memory_malloc(sizeof(wndmgr_alert_window_data_t));

    if(alert_window_data == NULL) {
        memory_free(frame_text);
        windowmanager_destroy_window(alert_window);
        return;
    }

    alert_window_data->self = alert_window;
    alert_window_data->on_enter = windowmanager_current_window->on_enter;
    alert_window_data->extra_data = windowmanager_current_window->extra_data;

    windowmanager_current_window->on_enter = wndmgr_alert_window_on_enter;
    windowmanager_current_window->extra_data = alert_window_data;
    windowmanager_current_window->has_alert = true;
    windowmanager_current_window->is_dirty = true;
}
