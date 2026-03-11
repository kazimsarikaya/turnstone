/**
 * @file wnd_options.64.c
 * @brief Window Manager Options Window
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
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
    WND_OPTIONS_TASK_VM_MANAGER,
    WND_OPTIONS_SPOOL_BROWSER,
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


typedef enum wnd_task_vm_manager_list_item_type_t {
    WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_TASK_VM_LIST,
    WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_TASK_VM_CREATE,
    WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_END,
} wnd_task_vm_manager_list_item_type_t;

const wnd_options_list_item_t wnd_task_manager_item_list[WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_END] = {
    [WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_TASK_VM_LIST] =    {
        .text   = "Task and VM List",
        .action = windowmanager_create_and_show_task_vm_list_window,
    },
    [WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_TASK_VM_CREATE] =    {
        .text   = "Create Task and VM",
        .action = windowmanager_create_and_show_task_vm_create_window,
    },
};

typedef enum wnd_primary_options_list_item_type_t {
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_VM_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF,
    WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END,
} wnd_primary_options_list_item_type_t;

const wnd_options_list_item_t wnd_primary_options_item_list[WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END] = {
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER] =    {
        .text   = "Spool Browser",
        .action = windowmanager_create_and_show_spool_browser_window,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_VM_MANAGER] =    {
        .text                = "Task and Virtual Machine Manager",
        .next_options_window = WND_OPTIONS_TASK_VM_MANAGER,
        .action              = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER] =    {
        .text   = "Network Manager",
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER] =    {
        .text   = "Turnstone Database Manager",
        .action = NULL,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT] =    {
        .text   = "Reboot",
        .action = wndmgr_reboot,
    },
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF] =    {
        .text   = "Power Off",
        .action = wndmgr_power_off,
    },
};

const wnd_options_list_t wnd_options_list[WND_OPTIONS_END] = {
    [WND_OPTIONS_PRIMARY] =    {
        .title       = "tOS Primary Options Menu",
        .items       = wnd_primary_options_item_list,
        .items_count = WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_END,
    },
    [WND_OPTIONS_TASK_VM_MANAGER] =    {
        .title       = "tOS Task and Virtual Machine Manager",
        .items       = wnd_task_manager_item_list,
        .items_count = WND_TASK_VM_MANAGER_LIST_ITEM_TYPE_END,
    },
};

window_t* windowmanager_create_primary_options_window(void) {
    window_t* pri_opt_wnd = windowmanager_create_options_window(WND_OPTIONS_PRIMARY);

    if(pri_opt_wnd == NULL) {
        return NULL;
    }

    return pri_opt_wnd;
}

static int8_t wndmgr_options_on_enter(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    list_t* inputs = wndmgr_get_input_values(wndmgr->current_window);

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
            windowmanager_insert_and_set_current_window(next_window);
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

    windowmanager_t* wndmgr = windowmanager_get_instance();

    const wnd_options_list_t* options_list = &wnd_options_list[option_window_type];

    window_top_window_t top_window = windowmanager_create_top_window(options_list->title, true);

    if(!top_window.main_window || !top_window.inside_window) {
        return NULL;
    }

    uint32_t font_width = wndmgr->font_width;
    uint32_t max_width  = top_window.inside_window->owner_rect.width;

    int32_t option_list_height = 0;
    rect_t rect;

    for(int64_t i = 0; i < options_list->items_count; i++) {

        char_t* option_number = strprintf("% 8d.", i);

        rect = wndmgr_calc_text_rect(option_number, max_width);

        rect.y = option_list_height;

        window_t* option_number_area = windowmanager_create_window(top_window.inside_window,
                                                                   rect,
                                                                   (color_t){.color = 0xFF2288FF},
                                                                   .text = option_number);

        memory_free(option_number);

        if(option_number_area == NULL) {
            windowmanager_destroy_window(top_window.main_window);
            return NULL;
        }

        char_t* option_text = strprintf("%s", options_list->items[i].text);

        rect = wndmgr_calc_text_rect(option_text,
                                     max_width - option_number_area->owner_rect.width - font_width);

        rect.x = option_number_area->owner_rect.width +  font_width;

        rect.y              = option_list_height;
        option_list_height += rect.height;

        window_t* option_text_area = windowmanager_create_window(top_window.inside_window,
                                                                 rect,
                                                                 (color_t){.color = 0xFF00FF00},
                                                                 .text = option_text);

        memory_free(option_text);

        if(option_text_area == NULL) {
            windowmanager_destroy_window(top_window.main_window);
            return NULL;
        }
    }

    top_window.inside_window->extra_data              = (void*)(uint64_t)option_window_type;
    top_window.inside_window->extra_data_is_allocated = false;
    top_window.inside_window->on_enter                = wndmgr_options_on_enter;

    return top_window.main_window;
}
