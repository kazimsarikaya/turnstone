/**
 * @file wnd_inputs.64.c
 * @brief Several small windows
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <windowmanager.h>
#include <strings.h>
#include <graphics/screen.h>

MODULE("turnstone.windowmanager");


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
