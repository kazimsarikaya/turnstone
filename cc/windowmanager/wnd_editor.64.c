/**
 * @file wnd_editor.64.c
 * @brief Window Manager Virtual Machine Manager
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <strings.h>
#include <spool.h>
#include <argumentparser.h>
#include <graphics/screen.h>
#include <logging.h>
#include <strings.h>
#include <math.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);


static window_t* wnd_create_textbox(char_t* text, window_t* parent, int64_t x_offset, color_t bg_color, color_t fg_color) {
    rect_t rect = windowmanager_calc_text_rect(text, 1900);

    rect.x = x_offset;

    window_t* window = windowmanager_create_window(parent,
                                                   text,
                                                   rect,
                                                   bg_color,
                                                   fg_color);

    if(window == NULL) {
        return NULL;
    }

    window->rect.y = parent->rect.y;
    window->is_visible = true;
    window->is_dirty = true;

    return window;
}

static window_t* wnd_create_editor_ruler(window_t* parent, int64_t start, int64_t top, color_t bg_color, color_t fg_color) {
    screen_info_t screen_info = screen_get_info();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    int64_t max_ruler = (screen_info.width / font_width) - 9;
    int64_t ruler_col_count = max_ruler;
    max_ruler += start;

    char_t* max_ruler_str = sprintf("%i", max_ruler);

    int64_t ruler_line_count = strlen(max_ruler_str);

    memory_free(max_ruler_str);

    char_t** ruler_lines = memory_malloc(ruler_col_count * sizeof(char_t*));

    if(ruler_lines == NULL) {
        return NULL;
    }

    for(int64_t i = 0; i < ruler_line_count; i++) {
        ruler_lines[i] = memory_malloc(ruler_col_count * sizeof(char_t));

        if(ruler_lines[i] == NULL) {
            for(int64_t j = 0; j < i; j++) {
                memory_free(ruler_lines[j]);
            }

            memory_free(ruler_lines);

            return NULL;
        }
    }

    int64_t ten_div = power(10, ruler_line_count - 1);

    for(int64_t i = 0; i < ruler_col_count; i++) {
        int64_t ruler_value = i + start;
        int64_t tmp_ten_div = ten_div;

        for(int64_t j = 0; j < ruler_line_count; j++) {
            int64_t ruler_digit = (ruler_value / tmp_ten_div) % 10;
            ruler_lines[j][i] = '0' + ruler_digit;
            tmp_ten_div /= 10;
        }
    }

    int64_t ruler_line_top = top;

    rect_t rect = {0, ruler_line_top, screen_info.width, font_height * ruler_line_count};

    window_t* ruler_window = windowmanager_create_window(parent,
                                                         NULL,
                                                         rect,
                                                         bg_color,
                                                         fg_color);

    if(ruler_window == NULL) {
        for(int64_t i = 0; i < ruler_line_count; i++) {
            memory_free(ruler_lines[i]);
        }

        memory_free(ruler_lines);

        return NULL;
    }

    ruler_window->is_visible = true;
    ruler_window->is_dirty = true;

    int64_t offset_x = 9 * font_width;

    for(int64_t i = 0; i < ruler_line_count; i++) {
        window_t* ruler_line_window = wnd_create_textbox(ruler_lines[i], ruler_window, offset_x, bg_color, fg_color);

        if(ruler_line_window == NULL) {
            for(int64_t j = 0; j < ruler_line_count; j++) {
                memory_free(ruler_lines[j]);
            }

            memory_free(ruler_lines);

            windowmanager_destroy_window(ruler_window);

            return NULL;
        }

        ruler_line_window->is_visible = true;
        ruler_line_window->is_dirty = true;

        ruler_line_window->rect.y = ruler_line_top;

        ruler_line_top += ruler_line_window->rect.height;
    }

    memory_free(ruler_lines);

    return ruler_window;
}

static window_t* wnd_create_numbered_line(int64_t line_number, const char_t* line, int64_t line_length, int64_t top, window_t* parent) {
    screen_info_t screen_info = screen_get_info();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window_t* window = windowmanager_create_window(parent,
                                                   NULL,
                                                   (rect_t){0,
                                                            top,
                                                            screen_info.width,
                                                            font_height},
                                                   (color_t){.color = 0x00000000},
                                                   (color_t){.color = 0xFFFFFFFF});


    if(window == NULL) {
        return NULL;
    }

    window->is_visible = true;
    window->is_dirty = true;

    char_t* line_number_str = sprintf("%08i ", line_number);

    window_t* line_number_window = wnd_create_textbox(line_number_str, window, 0, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000});

    if(line_number_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    line_number_window->is_visible = true;
    line_number_window->is_dirty = true;

    char_t* line_str = strndup(line, line_length);

    window_t* line_window = wnd_create_textbox(line_str, window, line_number_window->rect.width, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFFFFFFF});

    if(line_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    line_window->is_visible = true;
    line_window->is_dirty = true;

    window->rect.height = line_window->rect.height;
    window->rect.width = line_window->rect.width + line_number_window->rect.width;

    return window;
}

int8_t windowmanager_create_and_show_editor(const char_t* title, const char_t* text, boolean_t is_text_readonly) {
    screen_info_t screen_info = screen_get_info();

    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window->is_visible = true;
    window->is_dirty = true;
    window->is_writable = !is_text_readonly;

    char_t* title_str = strdup(title);

    rect_t rect = windowmanager_calc_text_rect(title_str, 2000);
    rect.x = (screen_info.width - rect.width) / 2;
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

    title_window->is_visible = true;
    title_window->is_dirty = true;

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    window_t* ruler_window = wnd_create_editor_ruler(window, 1, option_input_row->rect.y + option_input_row->rect.height + 2 * font_height, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000});

    if(ruler_window == NULL) {
        windowmanager_destroy_window(window);
        return -1;
    }

    int64_t* line_lengths = NULL;
    int64_t line_count = 0;

    char_t** lines = strsplit(text, '\n', &line_lengths, &line_count);

    int64_t top = ruler_window->rect.y + ruler_window->rect.height;
    int64_t max_height = screen_info.height - font_height * 2;


    for(int64_t i = 0; i < line_count; i++) {
        window_t* line_window = wnd_create_numbered_line(i + 1, lines[i], line_lengths[i], top, window);

        if(line_window == NULL) {
            memory_free(line_lengths);
            memory_free(lines);
            windowmanager_destroy_window(window);
            return -1;
        }

        line_window->rect.y = top;
        line_window->is_visible = true;
        line_window->is_dirty = true;

        top += line_window->rect.height;

        if(top > max_height) {
            break;
        }
    }

    memory_free(line_lengths);
    memory_free(lines);

    windowmanager_insert_and_set_current_window(window);


    return 0;
}
