/**
 * @file wnd_editor.64.c
 * @brief Window Manager Virtual Machine Manager
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_editor.h>
#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_utils.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_options.h>
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
    rect_t rect = windowmanager_calc_text_rect(text, parent->rect.width - x_offset);

    rect.x = x_offset;

    window_t* window = windowmanager_create_window(parent,
                                                   text,
                                                   rect,
                                                   bg_color,
                                                   fg_color);

    if(window == NULL) {
        return NULL;
    }

    return window;
}

static window_t* wnd_create_editor_ruler(window_t* parent, int64_t start, int64_t top, color_t bg_color, color_t fg_color) {
    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    int64_t max_ruler = (parent->rect.width / font_width) - 9;
    int64_t ruler_col_count = max_ruler;
    max_ruler += start;

    char_t* max_ruler_str = strprintf("%i", max_ruler);

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

    boolean_t show_upper = false;

    for(int64_t i = 0; i < ruler_col_count; i++) {
        int64_t ruler_value = i + start;
        show_upper = true;

        for(int64_t j = ruler_line_count - 1; j >= 0; j--) {
            int64_t ruler_digit = ruler_value  % 10;

            if(show_upper) {
                ruler_lines[j][i] = '0' + ruler_digit;
                show_upper = ruler_digit == 0;
            } else {
                ruler_lines[j][i] = ' ';
                show_upper = false;
            }

            ruler_value /= 10;
        }
    }

    int64_t ruler_line_top = top;

    rect_t rect = {0, ruler_line_top, parent->rect.width, font_height * ruler_line_count};

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

        ruler_line_window->rect.y = ruler_line_top;

        ruler_line_top += ruler_line_window->rect.height;
    }

    memory_free(ruler_lines);

    return ruler_window;
}

static window_t* wnd_create_numbered_line(int64_t line_number, const char_t* line, int64_t line_length, int64_t top, window_t* parent) {
    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window_t* window = windowmanager_create_window(parent,
                                                   NULL,
                                                   (rect_t){0,
                                                            top,
                                                            parent->rect.width,
                                                            font_height},
                                                   (color_t){.color = 0x00000000},
                                                   (color_t){.color = 0xFFFFFFFF});


    if(window == NULL) {
        return NULL;
    }

    char_t* line_number_str = strprintf("%08i ", line_number);

    window_t* line_number_window = wnd_create_textbox(line_number_str, window, 0, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000});

    if(line_number_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    char_t* line_str = strndup(line, line_length);

    window_t* line_window = wnd_create_textbox(line_str, window, line_number_window->rect.width, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFFFFFFF});

    if(line_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    window->rect.height = line_window->rect.height;
    window->rect.width = line_window->rect.width + line_number_window->rect.width;

    return window;
}

typedef struct wnd_editor_extra_data_t {
    window_t*     ruler_window;
    int64_t       rect_ruler_top;
    window_t*     editor_window;
    const char_t* text;
    boolean_t     is_text_readonly;
    int64_t       row_start;
    int64_t       col_start;
} wnd_editor_extra_data_t;

static int8_t wnd_editor_on_redraw(const window_event_t* event) {
    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window_t* window = event->window;

    wnd_editor_extra_data_t* extra_data = window->extra_data;

    const char_t* text = extra_data->text;

    int64_t rect_ruler_top = extra_data->rect_ruler_top;

    int64_t col_start = extra_data->col_start;

    if(extra_data->ruler_window != NULL) {
        windowmanager_destroy_child_window(window, extra_data->ruler_window);
    }

    window_t* ruler_window = wnd_create_editor_ruler(window, col_start + 1, rect_ruler_top, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000});

    if(ruler_window == NULL) {
        return -1;
    }

    extra_data->ruler_window = ruler_window;

    int64_t top = ruler_window->rect.y + ruler_window->rect.height;
    int64_t max_height = window->rect.height - font_height; // remove footer line

    rect_t rect_editor = {0, top, window->rect.width, max_height - top};

    if(extra_data->editor_window != NULL) {
        windowmanager_destroy_child_window(window, extra_data->editor_window);
    }

    window_t* editor_window = windowmanager_create_window(window,
                                                          NULL,
                                                          rect_editor,
                                                          (color_t){.color = 0x00000000},
                                                          (color_t){.color = 0xFFFFFFFF});

    if(editor_window == NULL) {
        return -1;
    }

    extra_data->editor_window = editor_window;

    int64_t* line_lengths = NULL;
    int64_t line_count = 0;

    char_t** lines = strsplit(text, '\n', &line_lengths, &line_count);

    top = 0;

    int64_t max_lines = editor_window->rect.height / font_height;

    int64_t print_line_count = MIN(line_count, max_lines);

    int64_t row_start = extra_data->row_start;

    for(int64_t i = 0; i < print_line_count; i++) {
        char_t* line = NULL;
        int64_t line_length = 0;

        if(row_start + i < line_count) {
            line = lines[row_start + i];
            line_length = line_lengths[row_start + i];

            if(col_start > line_length) {
                col_start = line_length;
            } else if(col_start < 0) {
                col_start = 0;
            }

            line += col_start;
            line_length -= col_start;
        }


        window_t* line_window = wnd_create_numbered_line(row_start + i + 1, line, line_length, top, editor_window);

        if(line_window == NULL) {
            memory_free(line_lengths);
            memory_free(lines);
            return -1;
        }

        top += line_window->rect.height;
    }

    memory_free(line_lengths);
    memory_free(lines);
    return 0;
}

static int8_t wnd_editor_on_scroll(const window_event_t* event) {
    window_t* window = event->window;

    wnd_editor_extra_data_t* extra_data = window->extra_data;

    int64_t row_start = extra_data->row_start;
    int64_t col_start = extra_data->col_start;

    if(event->type == WINDOW_EVENT_TYPE_SCROLL_UP) {
        if(row_start > 0) {
            row_start--;
        }
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_DOWN) {
        row_start++;
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_LEFT) {
        if(col_start > 0) {
            col_start--;
        }
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_RIGHT) {
        col_start++;
    }

    extra_data->row_start = row_start;
    extra_data->col_start = col_start;

    // window->on_redraw(event);
    window->is_dirty = true;

    return 0;
}

int8_t windowmanager_create_and_show_editor_window(const char_t* title, const char_t* text, boolean_t is_text_readonly) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    screen_info_t screen_info = screen_get_info();

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    window->is_writable = !is_text_readonly;

    char_t* title_str = strdup(title);

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

    int64_t rect_rule_top = option_input_row->rect.y + option_input_row->rect.height + font_height;


    wnd_editor_extra_data_t* extra_data = memory_malloc(sizeof(wnd_editor_extra_data_t));

    if(extra_data == NULL) {
        windowmanager_destroy_window(window);
        return -1;
    }

    extra_data->ruler_window = NULL;
    extra_data->rect_ruler_top = rect_rule_top;
    extra_data->editor_window = NULL;
    extra_data->text = text;
    extra_data->is_text_readonly = is_text_readonly;
    extra_data->row_start = 0;
    extra_data->col_start = 0;

    window->extra_data = extra_data;
    window->extra_data_is_allocated = true;
    window->on_redraw = wnd_editor_on_redraw;
    window->on_scroll = wnd_editor_on_scroll;

    windowmanager_insert_and_set_current_window(window);


    return 0;
}
