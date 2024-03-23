/**
 * @file 117.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>

MODULE("turnstone.windowmanager");

window_t* windowmanager_create_primary_options_window(void) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return NULL;
    }

    window->is_visible = true;
    window->is_dirty = true;

    const char_t* title = "Primary Options Menu";

    rect_t rect = windowmanager_calc_text_rect(title, 2000);
    rect.x = (VIDEO_GRAPHICS_WIDTH - rect.width) / 2;
    rect.y = FONT_HEIGHT;


    window_t* title_window = windowmanager_create_window(window,
                                                         title,
                                                         rect,
                                                         (color_t){.color = 0x00000000},
                                                         (color_t){.color = 0xFFFF8822});

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


    const char_t* input_label_text = "Option ===> ";

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

    const char_t* input_text = "____________________";

    rect = windowmanager_calc_text_rect(input_text, 2000);

    rect.x = option_input_label->rect.width + 2 * FONT_WIDTH;

    window_t* option_input_text = windowmanager_create_window(option_input_row,
                                                              input_text,
                                                              rect,
                                                              (color_t){.color = 0x00000000},
                                                              (color_t){.color = 0xFF00FF00});

    if(option_input_text == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    option_input_text->is_visible = true;
    option_input_text->is_dirty = true;


    return window;
}


