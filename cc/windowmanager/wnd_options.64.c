/**
 * @file 117.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <strings.h>

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

const char_t*const wnd_primary_options_list[] = {
    "Spool Browser",
    "Task Manager",
    "Virtual Machine Manager",
    "Network Manager",
    "Turnstone Database Manager",
    "Reboot",
    "Power Off",
    NULL,
};

const char_t* wnd_primary_options_title = "tOS Primary Options Menu";

window_t* windowmanager_create_primary_options_window(void) {
    return windowmanager_create_options_window(wnd_primary_options_title, wnd_primary_options_list);
}
