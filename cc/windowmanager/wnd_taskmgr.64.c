/**
 * @file wnd_taskmgr.64.c
 * @brief Task Manager Window
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_task_manager.h>
#include <windowmanager/wnd_options.h>
#include <windowmanager/wnd_utils.h>
#include <strings.h>
#include <cpu/task.h>
#include <graphics/screen.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* str);


static int8_t wnd_task_list_on_redraw(const window_event_t* event) {
    UNUSED(event);

    window_t* window = event->window;

    window_t* wnd_task_list_area = (window_t*)window->extra_data;

    if(!wnd_task_list_area) {
        return -1;
    }

    windowmanager_destroy_all_child_windows(wnd_task_list_area);

    buffer_t* task_list = task_build_task_list();

    if(!task_list) {
        return -1;
    }

    uint64_t buf_len = 0;

    task_list_item_t* task_list_items = (task_list_item_t*)buffer_get_all_bytes_and_destroy(task_list, &buf_len);

    if(!task_list_items) {
        return -1;
    }

    if(buf_len == 0) {
        memory_free(task_list_items);
        return 0;
    }

    int64_t task_list_item_count = buf_len / sizeof(task_list_item_t);

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    int64_t top = 0;

    color_t bg_color = (color_t){.color = 0x00000000};

    for(int64_t i = 0; i < task_list_item_count; i++) {
        task_list_item_t* item = &task_list_items[i];

        char_t* task_str = strprintf("      % 15llx% 15lli% 20llx% 10d% 10lli% 20lli %s",
                                     item->task_id,
                                     item->cpu_id,
                                     item->task_switch_count,
                                     item->state,
                                     item->message_queues,
                                     item->messages,
                                     item->task_name);

        if(i % 2) {
            bg_color.color = 0xFF181818;
        } else {
            bg_color.color = 0xFF000000;
        }

        window_t* wnd_task = windowmanager_create_window(wnd_task_list_area,
                                                         task_str,
                                                         (rect_t){0, top, wnd_task_list_area->rect.width, font_height},
                                                         bg_color,
                                                         (color_t){.color = 0xFF00FF00});

        if(!wnd_task) {
            memory_free(task_list_items);
            return -1;
        }

        top += font_height;
    }

    memory_free(task_list_items);

    return 0;
}

static int8_t wnd_task_list_on_scroll(const window_event_t* event) {
    UNUSED(event);
    return 0;
}


int8_t windowmanager_create_and_show_task_list_window(void) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    screen_info_t screen_info = screen_get_info();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    char_t* title_str = strdup("Task List");

    rect_t rect = windowmanager_calc_text_rect(title_str, screen_info.width);
    rect.x = (window->rect.width - rect.width) / 2;
    rect.y = font_height;


    window_t* title_window = windowmanager_create_window(window,
                                                         title_str,
                                                         rect,
                                                         (color_t){.color = 0x00000000},
                                                         (color_t){.color = 0xFF2288FF});

    if(title_window == NULL) {
        windowmanager_destroy_window(window);
        return -1;
    }

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    int64_t option_input_row_bottom = option_input_row->rect.y + option_input_row->rect.height;

    window_t* wnd_header = windowmanager_create_window(window,
                                                       NULL,
                                                       (rect_t){0,
                                                                option_input_row_bottom + font_height,
                                                                screen_info.width,
                                                                font_height},
                                                       (color_t){.color = 0x00000000},
                                                       (color_t){.color = 0xFFee9900});

    if(!wnd_header) {
        windowmanager_destroy_window(window);
        return -1;
    }

    char_t* header_text = strprintf(" %- 5s% 15s% 15s% 20s% 10s% 10s% 20s %s",
                                    "Cmd", "Task ID", "Cpu ID", "Switch Count",
                                    "State", "MQ Count", "Message Count", "Name");

    window_t* wnd_header_text = windowmanager_create_window(wnd_header,
                                                            header_text,
                                                            (rect_t){0, 0, screen_info.width - font_width, font_height},
                                                            (color_t){.color = 0xFF181818},
                                                            (color_t){.color = 0xFFee9900});


    if(!wnd_header_text) {
        windowmanager_destroy_window(window);
        return -1;
    }

    int64_t wnd_header_bottom = wnd_header->rect.y + wnd_header->rect.height;

    int64_t wnd_task_list_area_top = wnd_header_bottom;

    rect_t wnd_task_list_area_rect = {0,
                                      wnd_task_list_area_top,
                                      screen_info.width,
                                      screen_info.height - wnd_task_list_area_top - font_height};

    window_t* wnd_task_list_area = windowmanager_create_window(window,
                                                               NULL,
                                                               wnd_task_list_area_rect,
                                                               (color_t){.color = 0x00000000},
                                                               (color_t){.color = 0xFFee9900});

    if(!wnd_task_list_area) {
        windowmanager_destroy_window(window);
        return -1;
    }

    window->extra_data = (void*)wnd_task_list_area;
    window->extra_data_is_allocated = false;

    window->on_redraw = wnd_task_list_on_redraw;
    window->on_scroll = wnd_task_list_on_scroll;

    windowmanager_insert_and_set_current_window(window);

    return 0;
}
