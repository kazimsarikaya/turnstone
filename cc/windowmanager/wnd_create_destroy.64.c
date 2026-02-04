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
#include <time.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

window_t* windowmanager_create_top_window(void) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    window->id = wndmgr->next_window_id++;
    window->rect.x = 0;
    window->rect.y = 0;
    window->rect.width = wndmgr->screen_width;
    window->rect.height = wndmgr->screen_height;
    window->absolute_rect = window->rect;
    window->background_color.color = 0xFF000000;
    window->foreground_color.color = 0xFFFFFFFF;
    window->is_visible = true;
    window->is_dirty = true;

    return window;
}


window_t* windowmanager_create_window(window_t* parent, char_t* text, rect_t rect, color_t background_color, color_t foreground_color) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for window\n");
        return NULL;
    }

    window->id = wndmgr->next_window_id++;
    window->text = text;
    window->rect = rect;
    window->absolute_rect = (rect_t){rect.x + parent->rect.x, rect.y + parent->rect.y, rect.width, rect.height};
    window->background_color = background_color;
    window->foreground_color = foreground_color;
    window->is_visible = true;
    window->is_dirty = true;
    window->parent = parent;

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

static int8_t wndmgr_footer_fps_on_redraw(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    char_t* fps_str = strprintf("FPS: %.02f", (wndmgr->previous_render_time) ? (1000000.0f / (float32_t)wndmgr->previous_render_time) : 1000000.0f);

    memory_free(window->text);

    ((window_t*)window)->text = fps_str;

    return 0;
}

static int8_t wndmgr_create_footer(window_t* parent) {
    if(parent == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    rect_t rect = {0, parent->rect.height - wndmgr->font_height, parent->rect.width, wndmgr->font_height};

    window_t* footer = windowmanager_create_window(parent, NULL, rect, (color_t){.color = 0xFF282828}, (color_t){.color = 0xFFFFFFFF});

    if(footer == NULL) {
        return -1;
    }

    footer->is_always_redrawn = true;

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = strprintf("%02d:%02d:%02d %04d-%02d-%02d",
                                 tp.hours, tp.minutes, tp.seconds,
                                 tp.year, tp.month, tp.day);

    rect = windowmanager_calc_text_rect(time_str, wndmgr->screen_width);

    rect.x = parent->rect.width - rect.width - wndmgr->font_width;

    window_t* time_wnd = windowmanager_create_window(footer, time_str, rect, (color_t){.color = 0xFF282828}, (color_t){.color = 0xFF2288FF});

    if(time_wnd == NULL) {
        windowmanager_destroy_window(footer);
        return -1;
    }

    time_wnd->on_redraw = wndmgr_footer_time_on_redraw;

    uint32_t fps_wnd_width = wndmgr->font_width * 20;

    rect = (rect_t){wndmgr->font_width, 0, fps_wnd_width, wndmgr->font_height};

    window_t* fps_wnd = windowmanager_create_window(footer, strprintf("FPS: %.02f", (wndmgr->previous_render_time) ? (1000000.0f / (float32_t)wndmgr->previous_render_time) : 1000000.0f), rect, (color_t){.color = 0xFF282828}, (color_t){.color = 0xFF22FF22});

    if(fps_wnd == NULL) {
        windowmanager_destroy_window(footer);
        return -1;
    }

    fps_wnd->on_redraw = wndmgr_footer_fps_on_redraw;

    return 0;
}

void windowmanager_insert_and_set_current_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(wndmgr_create_footer(window) != 0) {
        return;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* next = wndmgr->current_window->next;

    window->next = next;

    if(next != NULL) {
        next->prev = window;
    }

    window->prev = wndmgr->current_window;
    wndmgr->current_window->next = window;

    wndmgr->current_window = window;
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

    if(!window->is_text_readonly) {
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

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* next = wndmgr->current_window->next;
    window_t* prev = window->prev;

    prev->next = next;

    if(next != NULL) {
        next->prev = prev;
    }

    wndmgr->current_window = prev;
    wndmgr->current_window->is_dirty = true;

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

    windowmanager_t* wndmgr = windowmanager_get_instance();

    wndmgr_alert_window_data_t* alert_window_data = (wndmgr_alert_window_data_t*)wndmgr->current_window->extra_data;
    window_t* alert_window = alert_window_data->self;

    if(alert_window == NULL) {
        return -1;
    }

    list_list_delete(wndmgr->current_window->children, alert_window);

    wndmgr->current_window->on_enter = alert_window_data->on_enter;
    wndmgr->current_window->extra_data = alert_window_data->extra_data;
    wndmgr->current_window->has_alert = false;
    wndmgr->current_window->is_dirty = true;

    windowmanager_destroy_window(alert_window);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void windowmanager_create_and_show_alert_window(windowmanager_alert_window_type_t type, const char_t* text) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    rect_t rect = windowmanager_calc_text_rect(text, 400);

    rect_t alert_rect = {0, 0, rect.width + wndmgr->font_width * 4, rect.height + wndmgr->font_height * 4};

    alert_rect.x = (wndmgr->screen_height - alert_rect.width) / 2;
    alert_rect.y = (wndmgr->screen_height - alert_rect.height) / 2;

    int32_t linecharcount = alert_rect.width / wndmgr->font_width;
    int32_t linecount = alert_rect.height / wndmgr->font_height;

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

    window_t* alert_window = windowmanager_create_window(wndmgr->current_window, frame_text, alert_rect, background_color, foreground_color);

    if(alert_window == NULL) {
        memory_free(frame_text);
        return;
    }

    rect.x = 2 * wndmgr->font_width;
    rect.y = 2 * wndmgr->font_height;

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
    alert_window_data->on_enter = wndmgr->current_window->on_enter;
    alert_window_data->extra_data = wndmgr->current_window->extra_data;

    wndmgr->current_window->on_enter = wndmgr_alert_window_on_enter;
    wndmgr->current_window->extra_data = alert_window_data;
    wndmgr->current_window->has_alert = true;
    wndmgr->current_window->is_dirty = true;
}
#pragma GCC diagnostic pop
