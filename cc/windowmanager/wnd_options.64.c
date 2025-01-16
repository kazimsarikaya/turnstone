/**
 * @file wnd_options.64.c
 * @brief Window Manager Options Window
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_options.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <windowmanager/wnd_vmmgr.h>
#include <windowmanager/wnd_spool_browser.h>
#include <windowmanager/wnd_task_manager.h>
#include <windowmanager/wnd_misc.h>
#include <strings.h>
#include <argumentparser.h>
#include <graphics/screen.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

typedef int8_t (*wndmgr_opt_action_f)(void);

typedef enum wnd_options_windows_t {
    WND_OPTIONS_NONE,
    WND_OPTIONS_PRIMARY,
    WND_OPTIONS_TASK_MANAGER,
    WND_OPTIONS_SPOOL_BROWSER,
    WND_OPTIONS_VIRTUAL_MACHINE_MANAGER,
    WND_OPTIONS_NETWORK_MANAGER,
    WND_OPTIONS_TURNSTONE_DATABASE_MANAGER,
    WND_OPTIONS_END,
} wnd_options_windows_t;

static window_t* windowmanager_create_options_window(wnd_options_windows_t option_window_type);

typedef struct wnd_options_list_item_t {
    const char_t*         text;
    wnd_options_windows_t next_options_window;
    wndmgr_opt_action_f   action;
} wnd_options_list_item_t;

typedef struct wnd_options_list_t {
    const char_t*                  title;
    const wnd_options_list_item_t* items;
    int64_t                        items_count;
} wnd_options_list_t;


typedef enum wnd_task_manager_list_item_type_t {
    WND_TASK_MANAGER_LIST_ITEM_TYPE_TASK_LIST,
    WND_TASK_MANAGER_LIST_ITEM_TYPE_END,
} wnd_task_manager_list_item_type_t;

const wnd_options_list_item_t wnd_task_manager_item_list[WND_TASK_MANAGER_LIST_ITEM_TYPE_END] = {
    [WND_TASK_MANAGER_LIST_ITEM_TYPE_TASK_LIST] =    {
        .text = "Task List",
        .action = windowmanager_create_and_show_task_list_window,
    },
};

typedef enum wnd_primary_options_list_item_type_t {
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_VIRTUAL_MACHINE_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END,
} wnd_primary_options_list_item_type_t;

const wnd_options_list_item_t wnd_primary_options_item_list[WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END] = {
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER] =    {
        .text = "Spool Browser",
        .action = windowmanager_create_and_show_spool_browser_window,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_MANAGER] =    {
        .text = "Task Manager",
        .next_options_window = WND_OPTIONS_TASK_MANAGER,
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_VIRTUAL_MACHINE_MANAGER] =    {
        .text = "Virtual Machine Manager",
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER] =    {
        .text = "Network Manager",
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER] =    {
        .text = "Turnstone Database Manager",
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT] =    {
        .text = "Reboot",
        .action = wndmgr_reboot,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF] =    {
        .text = "Power Off",
        .action = wndmgr_power_off,
    },
};

const wnd_options_list_t wnd_options_list[WND_OPTIONS_END] = {
    [WND_OPTIONS_PRIMARY] =    {
        .title = "tOS Primary Options Menu",
        .items = wnd_primary_options_item_list,
        .items_count = WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END,
    },
    [WND_OPTIONS_TASK_MANAGER] =    {
        .title = "tOS Task Manager",
        .items = wnd_task_manager_item_list,
        .items_count = WND_TASK_MANAGER_LIST_ITEM_TYPE_END,
    },
};

window_t* windowmanager_create_primary_options_window(void) {
    window_t* pri_opt_wnd = windowmanager_create_options_window(WND_OPTIONS_PRIMARY);

    if(pri_opt_wnd == NULL) {
        return NULL;
    }

    return pri_opt_wnd;
}

window_t* windowmanager_add_option_window(window_t* parent, rect_t pos) {
    screen_info_t screen_info = screen_get_info();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window_t* option_input_row = windowmanager_create_window(parent,
                                                             NULL,
                                                             (rect_t){font_width,
                                                                      pos.y + pos.height + 2 * font_height,
                                                                      screen_info.width - font_width,
                                                                      font_height},
                                                             (color_t){.color = 0x00000000},
                                                             (color_t){.color = 0xFF00FF00});

    if(option_input_row == NULL) {
        return NULL;
    }

    char_t* input_label_text = strdup("Option ===> ");

    rect_t rect = windowmanager_calc_text_rect(input_label_text, 2000);

    window_t* option_input_label = windowmanager_create_window(option_input_row,
                                                               input_label_text,
                                                               rect,
                                                               (color_t){.color = 0x00000000},
                                                               (color_t){.color = 0xFF00FF00});

    if(option_input_label == NULL) {
        return NULL;
    }

    char_t* input_text = strdup("____________________");

    rect = windowmanager_calc_text_rect(input_text, 2000);

    rect.x = option_input_label->rect.width + 2 * font_width;

    window_t* option_input_text = windowmanager_create_window(option_input_row,
                                                              input_text,
                                                              rect,
                                                              (color_t){.color = 0x00000000},
                                                              (color_t){.color = 0xFFFF0000});

    if(option_input_text == NULL) {
        return NULL;
    }

    option_input_text->is_writable = true;
    option_input_text->input_length = strlen(input_text);
    option_input_text->input_id = "option";

    return option_input_row;
}

static int8_t wndmgr_options_on_enter(const window_event_t* event) {
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

    window_input_value_t* input = (window_input_value_t*)list_queue_pop(inputs);

    argument_parser_t argparser = {input->value, 0};

    char_t* option = argument_parser_advance(&argparser);

    if(option == NULL) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    if(strlen(option) == 0) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    wnd_primary_options_list_item_type_t option_number = atoi(option);

    wnd_options_windows_t window_type = (wnd_options_windows_t)(uint64_t)window->extra_data;

    if(window_type >= WND_OPTIONS_END) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    const wnd_options_list_t* options = &wnd_options_list[window_type];

    if(options == NULL) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    if(option_number >= options->items_count) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    const wnd_options_list_item_t* item = &options->items[option_number];

    if(item == NULL) {
        memory_free(input->value);
        memory_free(input);

        list_destroy(inputs);

        return -1;
    }

    int8_t ret = -1;

    if(item->next_options_window != WND_OPTIONS_NONE) {
        window_t* next_window = windowmanager_create_options_window(item->next_options_window);

        if(next_window) {
            window_t* next = windowmanager_create_options_window(item->next_options_window);
            windowmanager_insert_and_set_current_window(next);
            ret = 0;
        }
    } else if(item->action) {
        ret = item->action();
    }

    memory_free(input->value);
    memory_free(input);

    list_destroy(inputs);

    return ret;
}

static window_t* windowmanager_create_options_window(wnd_options_windows_t option_window_type) {
    if(option_window_type >= WND_OPTIONS_END) {
        return NULL;
    }

    const wnd_options_list_t* options_list = &wnd_options_list[option_window_type];

    screen_info_t screen_info = screen_get_info();

    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return NULL;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    char_t* title_str = strdup(options_list->title);

    rect_t rect = windowmanager_calc_text_rect(options_list->title, screen_info.width);
    rect.x = (screen_info.width - rect.width) / 2;
    rect.y = font_height;


    window_t* title_window = windowmanager_create_window(window,
                                                         title_str,
                                                         rect,
                                                         (color_t){.color = 0x00000000},
                                                         (color_t){.color = 0xFF2288FF});

    if(title_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    window_t* wnd_option_list = windowmanager_create_window(window,
                                                            NULL,
                                                            (rect_t){font_width,
                                                                     option_input_row->rect.y + option_input_row->rect.height + 2 * font_height,
                                                                     screen_info.width - font_width,
                                                                     0},
                                                            (color_t){.color = 0x00000000},
                                                            (color_t){.color = 0xFF00FF00});

    if(wnd_option_list == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    int32_t option_list_height = 0;

    for(int64_t i = 0; i < options_list->items_count; i++) {

        char_t* option_number = strprintf("% 8d.", i);

        rect = windowmanager_calc_text_rect(option_number, 2000);

        rect.y = option_list_height;

        window_t* option_number_area = windowmanager_create_window(wnd_option_list,
                                                                   option_number,
                                                                   rect,
                                                                   (color_t){.color = 0x00000000},
                                                                   (color_t){.color = 0xFF2288FF});

        if(option_number_area == NULL) {
            windowmanager_destroy_window(window);
            return NULL;
        }

        char_t* option_text = strprintf("%s", options_list->items[i].text);

        rect = windowmanager_calc_text_rect(option_text, screen_info.width - option_number_area->rect.width - font_width);

        rect.x = option_number_area->rect.width +  font_width;

        rect.y = option_list_height;
        option_list_height += rect.height;

        window_t* option_text_area = windowmanager_create_window(wnd_option_list,
                                                                 option_text,
                                                                 rect,
                                                                 (color_t){.color = 0x00000000},
                                                                 (color_t){.color = 0xFF00FF00});

        if(option_text_area == NULL) {
            windowmanager_destroy_window(window);
            return NULL;
        }
    }

    wnd_option_list->rect.height = option_list_height;

    window->extra_data = (void*)(uint64_t)option_window_type;
    window->extra_data_is_allocated = false;

    window->on_enter = wndmgr_options_on_enter;

    return window;
}
