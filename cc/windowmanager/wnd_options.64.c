/**
 * @file 117.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <strings.h>
#include <argumentparser.h>
#include <acpi.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

static window_t* windowmanager_create_options_window(const char_t* title, const char_t*const* options_list) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return NULL;
    }

    window->is_visible = true;
    window->is_dirty = true;

    char_t* title_str = strdup(title);

    rect_t rect = windowmanager_calc_text_rect(title, 2000);
    rect.x = (VIDEO_GRAPHICS_WIDTH - rect.width) / 2;
    rect.y = FONT_HEIGHT;


    window_t* title_window = windowmanager_create_window(window,
                                                         title_str,
                                                         rect,
                                                         (color_t){.color = 0x00000000},
                                                         (color_t){.color = 0xFF2288FF});

    if(title_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    title_window->is_visible = true;
    title_window->is_dirty = true;

    window_t* option_input_row = windowmanager_create_window(window,
                                                             NULL,
                                                             (rect_t){FONT_WIDTH,
                                                                      title_window->rect.y + title_window->rect.height + 2 * FONT_HEIGHT,
                                                                      VIDEO_GRAPHICS_WIDTH - FONT_WIDTH,
                                                                      FONT_HEIGHT},
                                                             (color_t){.color = 0x00000000},
                                                             (color_t){.color = 0xFF00FF00});

    if(option_input_row == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    option_input_row->is_visible = true;
    option_input_row->is_dirty = true;


    char_t* input_label_text = strdup("Option ===> ");

    rect = windowmanager_calc_text_rect(input_label_text, 2000);

    window_t* option_input_label = windowmanager_create_window(option_input_row,
                                                               input_label_text,
                                                               rect,
                                                               (color_t){.color = 0x00000000},
                                                               (color_t){.color = 0xFF00FF00});

    if(option_input_label == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    option_input_label->is_visible = true;
    option_input_label->is_dirty = true;

    char_t* input_text = strdup("____________________");

    rect = windowmanager_calc_text_rect(input_text, 2000);

    rect.x = option_input_label->rect.width + 2 * FONT_WIDTH;

    window_t* option_input_text = windowmanager_create_window(option_input_row,
                                                              input_text,
                                                              rect,
                                                              (color_t){.color = 0x00000000},
                                                              (color_t){.color = 0xFFFF0000});

    if(option_input_text == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    option_input_text->is_visible = true;
    option_input_text->is_dirty = true;
    option_input_text->is_writable = true;
    option_input_text->input_length = strlen(input_text);
    option_input_text->input_id = "option";

    window_t* option_list = windowmanager_create_window(window,
                                                        NULL,
                                                        (rect_t){FONT_WIDTH,
                                                                 option_input_row->rect.y + option_input_row->rect.height + 2 * FONT_HEIGHT,
                                                                 VIDEO_GRAPHICS_WIDTH - FONT_WIDTH,
                                                                 0},
                                                        (color_t){.color = 0x00000000},
                                                        (color_t){.color = 0xFF00FF00});

    if(option_list == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    option_list->is_visible = true;
    option_list->is_dirty = true;

    int32_t option_list_height = 0;

    for(size_t i = 0; options_list[i]; i++) {

        char_t* option_number = sprintf("% 8d.", i);

        rect = windowmanager_calc_text_rect(option_number, 2000);

        rect.y = option_list_height;

        window_t* option_number_area = windowmanager_create_window(option_list,
                                                                   option_number,
                                                                   rect,
                                                                   (color_t){.color = 0x00000000},
                                                                   (color_t){.color = 0xFF2288FF});

        if(option_number_area == NULL) {
            windowmanager_destroy_window(window);
            return NULL;
        }

        option_number_area->is_visible = true;
        option_number_area->is_dirty = true;

        char_t* option_text = sprintf("%s", options_list[i]);

        rect = windowmanager_calc_text_rect(option_text, 2000);

        rect.x = option_number_area->rect.width +  FONT_WIDTH;

        rect.y = option_list_height;
        option_list_height += rect.height;

        window_t* option_text_area = windowmanager_create_window(option_list,
                                                                 option_text,
                                                                 rect,
                                                                 (color_t){.color = 0x00000000},
                                                                 (color_t){.color = 0xFF00FF00});

        if(option_text_area == NULL) {
            windowmanager_destroy_window(window);
            return NULL;
        }

        option_text_area->is_visible = true;
        option_text_area->is_dirty = true;
    }

    option_list->rect.height = option_list_height;

    return window;
}

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

const char_t*const wnd_primary_options_list[] = {
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER] =    "Spool Browser",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_MANAGER] =    "Task Manager",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_VIRTUAL_MACHINE_MANAGER] =    "Virtual Machine Manager",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER] =    "Network Manager",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER] =    "Turnstone Database Manager",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT] =    "Reboot",
    [WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF] =    "Power Off",
    NULL,
};

const char_t* wnd_primary_options_title = "tOS Primary Options Menu";

static int8_t wndmgr_pri_opts_on_enter(const window_t* window) {
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

    wnd_primary_options_list_item_type_t option_number = atoi(option);

    video_text_print("Option selected: ");
    video_text_print(option);
    video_text_print(" ");
    video_text_print(wnd_primary_options_list[option_number]);
    video_text_print("\n");

    switch(option_number) {
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_SPOOL_BROWSER:
        return windowmanager_create_and_show_spool_browser_window();
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TASK_MANAGER:
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_VIRTUAL_MACHINE_MANAGER:
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_NETWORK_MANAGER:
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_TURNSTONE_DATABASE_MANAGER:
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_REBOOT:
        acpi_reset();
        break;
    case WND_PRIMARY_OPTIONS_LIST_ITEM_TYPE_POWER_OFF:
        acpi_poweroff();
        break;
    default:
        video_text_print("Invalid option selected\n");
        break;
    }


    memory_free(input->value);
    memory_free(input);

    list_destroy(inputs);

    return 0;
}

window_t* windowmanager_create_primary_options_window(void) {
    window_t* pri_opt_wnd = windowmanager_create_options_window(wnd_primary_options_title, wnd_primary_options_list);

    if(pri_opt_wnd == NULL) {
        return NULL;
    }

    pri_opt_wnd->on_enter = wndmgr_pri_opts_on_enter;

    return pri_opt_wnd;
}
