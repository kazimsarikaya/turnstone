/**
 * @file wnd_taskmgr.64.c
 * @brief Task Manager Window
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <strings.h>
#include <cpu/task.h>
#include <graphics/screen.h>
#include <hypervisor/hypervisor.h>
#include <logging.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* str);


static int8_t wnd_task_list_on_predraw(const window_event_t* event) {
    UNUSED(event);

    windowmanager_t* wndmgr = windowmanager_get_instance();

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

    task_list_item_t* task_list_items = (task_list_item_t*)(void*)buffer_get_all_bytes_and_destroy(task_list, &buf_len);

    if(!task_list_items) {
        return -1;
    }

    if(buf_len == 0) {
        memory_free(task_list_items);
        return 0;
    }

    int64_t task_list_item_count = buf_len / sizeof(task_list_item_t);

    uint32_t font_height = wndmgr->font_height;

    int64_t top = 0;

    color_t bg_color = (color_t){.color = 0x00000000};

    for(int64_t i = 0; i < task_list_item_count; i++) {
        task_list_item_t* item = &task_list_items[i];

        char_t* task_str = strprintf("      % 15llx% 15lli% 20llx% 10d% 10lli% 20lli %s %s",
                                     item->task_id,
                                     item->cpu_id,
                                     item->task_switch_count,
                                     item->state,
                                     item->message_queues,
                                     item->messages,
                                     item->has_virtual_machine ? "  Y  " : "  N  ",
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


int8_t windowmanager_create_and_show_task_vm_list_window(void) {
    window_top_window_t top_window = windowmanager_create_top_window("Task List", true);

    if(!top_window.main_window || !top_window.inside_window) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;
    uint32_t screen_width  = wndmgr->screen_width;
    uint32_t screen_height = wndmgr->screen_height;

    window_t* window = top_window.inside_window;

    window_t* wnd_header = windowmanager_create_window(window,
                                                       NULL,
                                                       (rect_t){0,
                                                                0,
                                                                screen_width,
                                                                font_height},
                                                       (color_t){.color = 0x00000000},
                                                       (color_t){.color = 0xFFee9900});

    if(!wnd_header) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    char_t* header_text = strprintf(" %- 5s% 15s% 15s% 20s% 10s% 10s% 20s %s %s",
                                    "Cmd", "Task ID", "Cpu ID", "Switch Count",
                                    "State", "MQ Count", "Message Count", "Is VM", "Name");

    window_t* wnd_header_text = windowmanager_create_window(wnd_header,
                                                            header_text,
                                                            (rect_t){0, 0, screen_width - font_width, font_height},
                                                            (color_t){.color = 0xFF181818},
                                                            (color_t){.color = 0xFFee9900});


    if(!wnd_header_text) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    rect_t wnd_task_list_area_rect = {0,
                                      font_height,
                                      screen_width,
                                      screen_height - wnd_header_text->rect.y - font_height - font_height};

    window_t* wnd_task_list_area = windowmanager_create_window(window,
                                                               NULL,
                                                               wnd_task_list_area_rect,
                                                               (color_t){.color = 0x00000000},
                                                               (color_t){.color = 0xFFee9900});

    if(!wnd_task_list_area) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    wnd_task_list_area->is_always_redrawn = true;

    wnd_task_list_area->extra_data              = (void*)wnd_task_list_area;
    wnd_task_list_area->extra_data_is_allocated = false;

    wnd_task_list_area->on_predraw = wnd_task_list_on_predraw;
    wnd_task_list_area->on_scroll  = wnd_task_list_on_scroll;

    windowmanager_insert_and_set_current_window(top_window.main_window);

    return 0;
}

static int8_t wndmgr_create_vm_on_enter(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    list_t* inputs = windowmanager_get_input_values(window);

    if(!list_size(inputs)) {
        list_destroy(inputs);
        return -1;
    }

    char_t* entry_point = NULL;
    uint64_t heap_size  = 0;
    uint64_t stack_size = 0;

    for(uint64_t i = 0; i < list_size(inputs); i++) {
        window_input_value_t* input = (window_input_value_t*)list_get_data_at_position(inputs, i);

        if(input == NULL) {
            continue;
        }

        if(input->value == NULL) {
            memory_free(input);
            continue;
        }

        if(strlen(input->value) == 0) {
            memory_free(input->value);
            memory_free(input);
            continue;
        }

        if(strcmp(input->id, "entry_point") == 0) {
            entry_point = strdup(input->value);
        } else if(strcmp(input->id, "heap_size") == 0) {
            heap_size = atou(input->value);
        } else if(strcmp(input->id, "stack_size") == 0) {
            stack_size = atou(input->value);
        }

        memory_free(input->value);
        memory_free(input);

    }

    list_destroy(inputs);

    if(entry_point == NULL || heap_size == 0 || stack_size == 0) {
        return -1;
    }

    if(strlen(entry_point) == 0) {
        memory_free(entry_point);
        return -1;
    }

    int8_t ret = hypervisor_vm_create(entry_point, heap_size, stack_size);

    memory_free(entry_point);

    if(ret != 0) {
        windowmanager_create_and_show_alert_window(WINDOWMANAGER_ALERT_WINDOW_TYPE_ERROR,
                                                   "Failed to create VM. Please check the parameters.");
    } else {
        windowmanager_create_and_show_alert_window(WINDOWMANAGER_ALERT_WINDOW_TYPE_INFO,
                                                   "VM created successfully.");
    }

    return 0;
}

int8_t windowmanager_create_and_show_task_vm_create_window(void) {
    window_top_window_t top_window = windowmanager_create_top_window("Create Task or VM", true);

    if(!top_window.main_window || !top_window.inside_window) {
        return -1;
    }


    windowmanager_t* wndmgr = windowmanager_get_instance();


    uint32_t font_height = wndmgr->font_height;

    window_t* window = top_window.inside_window;


    rect_t wnd_entry_point_rect = {
        .x      = 0,
        .y      = 0,
        .width  = window->rect.width,
        .height = font_height,
    };

    window_t* wnd_entry_point = windowmanager_command_input_window(window, wnd_entry_point_rect,
                                                                   "Entry Point",
                                                                   WINDOWMANAGER_COMMAND_INPUT_TEXT,
                                                                   "entry_point",
                                                                   "Entry Point function name of VM");

    if(!wnd_entry_point) {
        windowmanager_destroy_window(window);
        return -1;
    }

    rect_t wnd_heap_size_rect = {
        .x      = 0,
        .y      = font_height,
        .width  = window->rect.width,
        .height = font_height,
    };

    window_t* wnd_heap_size = windowmanager_command_input_window(window, wnd_heap_size_rect,
                                                                 "Heap Size",
                                                                 "0x0000000001000000", // 16MiB
                                                                 "heap_size",
                                                                 "Heap size of VM");

    if(!wnd_heap_size) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    rect_t wnd_stack_size_rect = {
        .x      = 0,
        .y      =  2 * font_height,
        .width  = window->rect.width,
        .height = font_height,
    };

    window_t* wnd_stack_size = windowmanager_command_input_window(window, wnd_stack_size_rect,
                                                                  "Stack Size",
                                                                  "0x0000000000200000", // 2MiB
                                                                  "stack_size",
                                                                  "Stack size of VM");

    if(!wnd_stack_size) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    top_window.inside_window->on_enter = wndmgr_create_vm_on_enter;


    windowmanager_insert_and_set_current_window(top_window.main_window);

    return 0;
}
