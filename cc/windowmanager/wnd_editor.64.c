/**
 * @file wnd_editor.64.c
 * @brief Window Manager Virtual Machine Manager
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_types.h>
#include <windowmanager/wnd_utils.h>
#include <windowmanager/wnd_create_destroy.h>
#include <strings.h>
#include <spool.h>
#include <argumentparser.h>
#include <graphics/screen.h>
#include <logging.h>
#include <strings.h>
#include <math.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);


static window_t* wnd_create_textbox(char16_t* text, window_t* parent,
                                    int64_t x_offset, int64_t y_offset,
                                    color_t bg_color, color_t fg_color,
                                    boolean_t is_writable) {
    rect_t rect = wndmgr_calc_text_rect(text, parent->owner_rect.width - x_offset);

    rect.x = x_offset;
    rect.y = y_offset;

    window_t* window = windowmanager_create_window(parent,
                                                   rect,
                                                   fg_color,
                                                   bg_color,
                                                   text,
                                                   .is_writable     = is_writable,
                                                   .is_single_sheet = is_writable);

    if(window == NULL) {
        return NULL;
    }

    return window;
}

static window_t* wnd_create_editor_ruler(windowmanager_t* wndmgr, window_t* parent, int64_t start, int64_t top, color_t bg_color, color_t fg_color) {
    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;

    int64_t max_ruler       = (parent->owner_rect.width / font_width) - 9;
    int64_t ruler_col_count = max_ruler;
    max_ruler += start;

    char_t* max_ruler_str = strprintf("%lli", max_ruler);

    int64_t ruler_line_count = strlen(max_ruler_str);

    memory_free(max_ruler_str);

    char16_t** ruler_lines = memory_malloc(ruler_col_count * sizeof(char16_t*));

    if(!ruler_lines) {
        return NULL;
    }

    for(int64_t i = 0; i < ruler_line_count; i++) {
        ruler_lines[i] = memory_malloc(ruler_col_count * sizeof(char16_t));

        if(!ruler_lines[i]) {
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
                show_upper        = ruler_digit == 0;
            } else {
                ruler_lines[j][i] = ' ';
                show_upper        = false;
            }

            ruler_value /= 10;
        }
    }

    int64_t ruler_line_top = top;

    rect_t rect = {0, ruler_line_top, parent->owner_rect.width, font_height * ruler_line_count};

    window_t* ruler_window = windowmanager_create_window(parent,
                                                         rect,
                                                         fg_color,
                                                         bg_color);

    if(ruler_window == NULL) {
        for(int64_t i = 0; i < ruler_line_count; i++) {
            memory_free(ruler_lines[i]);
        }

        memory_free(ruler_lines);

        return NULL;
    }

    int64_t offset_x = 9 * font_width;
    int64_t offset_y = 0;

    for(int64_t i = 0; i < ruler_line_count; i++) {
        window_t* ruler_line_window = wnd_create_textbox(ruler_lines[i], ruler_window,
                                                         offset_x, offset_y,
                                                         bg_color, fg_color, false);

        memory_free(ruler_lines[i]);

        if(ruler_line_window == NULL) {

            memory_free(ruler_lines);

            windowmanager_destroy_window(ruler_window);

            return NULL;
        }

        offset_y += ruler_line_window->owner_rect.height;
    }

    memory_free(ruler_lines);

    return ruler_window;
}

static window_t* wnd_create_numbered_line(windowmanager_t* wndmgr, int64_t line_number, const char16_t* line, int64_t line_length, int64_t top, window_t* parent, boolean_t is_writable) {
    uint32_t font_height = wndmgr->font_height;

    window_t* window = windowmanager_create_window(parent,
                                                   (rect_t){0,
                                                            top,
                                                            parent->owner_rect.width,
                                                            font_height},
                                                   (color_t){.color = 0xFFFFFFFF});


    if(window == NULL) {
        return NULL;
    }

    char16_t* line_number_str = wstrprintf("%08lli ", line_number);

    // TODO: allow editor commands to be entered in line number area, for example to set breakpoints, bookmarks, etc.
    window_t* line_number_window = wnd_create_textbox(line_number_str, window, 0, 0,
                                                      (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000}, false);

    if(line_number_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    char16_t* line_str = wstrndup(line, line_length);

    window_t* line_window = wnd_create_textbox(line_str, window, line_number_window->owner_rect.width, 0,
                                               (color_t){.color = 0x00000000}, (color_t){.color = 0xFFFFFFFF},
                                               is_writable);

    if(line_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    return window;
}

typedef struct wnd_editor_extra_data_t {
    window_t*       ruler_window;
    window_t*       editor_window;
    const char16_t* text;
    boolean_t       is_text_readonly;
    boolean_t       is_editor_readonly;
    int64_t         row_start;
    int64_t         col_start;
} wnd_editor_extra_data_t;

static int8_t wnd_editor_on_predraw(const window_event_t* event) {
    window_t* window = event->window;

    if(!wndmgr_is_window_dirty(window)) {
        return 0;
    }

    windowmanager_t* wndmgr = window->wndmgr;

    uint32_t font_height = wndmgr->font_height;


    wnd_editor_extra_data_t* extra_data = window->extra_data;

    const char16_t* text = extra_data->text;


    int64_t col_start = extra_data->col_start;

    if(extra_data->ruler_window != NULL) {
        windowmanager_destroy_child_window(window, extra_data->ruler_window);
    }

    window_t* ruler_window = wnd_create_editor_ruler(wndmgr, window, col_start + 1, 0, (color_t){.color = 0x00000000}, (color_t){.color = 0xFFF00000});

    if(ruler_window == NULL) {
        return -1;
    }

    extra_data->ruler_window = ruler_window;

    int64_t top        = ruler_window->owner_rect.y + ruler_window->owner_rect.height;
    int64_t max_height = window->owner_rect.height - font_height; // remove footer line

    rect_t rect_editor = {0, top, window->owner_rect.width, max_height - top};

    if(extra_data->editor_window != NULL) {
        windowmanager_destroy_child_window(window, extra_data->editor_window);
    }

    window_t* editor_window = windowmanager_create_window(window,
                                                          rect_editor);

    if(editor_window == NULL) {
        return -1;
    }

    extra_data->editor_window = editor_window;

    int64_t* line_lengths = NULL;
    int64_t line_count    = 0;

    char16_t** lines = wstrsplit(text, '\n', &line_lengths, &line_count);

    top = 0;

    int64_t max_lines = editor_window->owner_rect.height / font_height;

    int64_t print_line_count = MIN(line_count, max_lines);

    int64_t row_start = extra_data->row_start;

    if(row_start + max_lines > line_count) {
        row_start = line_count - max_lines;

        if(row_start < 0) {
            row_start = 0;
        }

        extra_data->row_start = row_start;
    }

    for(int64_t i = 0; i < print_line_count; i++) {
        char16_t* line      = NULL;
        int64_t line_length = 0;

        line        = lines[row_start + i];
        line_length = line_lengths[row_start + i];

        if(col_start > line_length) {
            col_start = line_length;
        } else if(col_start < 0) {
            col_start = 0;
        }

        line        += col_start;
        line_length -= col_start;

        window_t* line_window = wnd_create_numbered_line(wndmgr, row_start + i + 1,
                                                         line, line_length,
                                                         top,
                                                         editor_window,
                                                         !extra_data->is_text_readonly && !extra_data->is_editor_readonly);

        if(line_window == NULL) {
            memory_free(line_lengths);
            memory_free(lines);
            return -1;
        }

        top += line_window->owner_rect.height;
    }

    memory_free(line_lengths);
    memory_free(lines);
    return 0;
}

static int8_t wnd_editor_on_scroll(const window_event_t* event) {
    window_t* window = event->window;

    if(!window) {
        video_text_print("No window in scroll event\n");
        return -1;
    }

    wnd_editor_extra_data_t* extra_data = window->extra_data;

    if(!extra_data) {
        video_text_print("No extra data in scroll event\n");
        return -1;
    }

    int64_t row_start = extra_data->row_start;
    int64_t col_start = extra_data->col_start;

    if(event->type == WINDOW_EVENT_TYPE_SCROLL_UP) {
        if(row_start > 0) {
            row_start--;
            wndmgr_mark_all_windows_dirty(window);
        }
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_DOWN) {
        row_start++;
        wndmgr_mark_all_windows_dirty(window);
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_LEFT) {
        if(col_start > 0) {
            col_start--;
            wndmgr_mark_all_windows_dirty(window);
        }
    } else if(event->type == WINDOW_EVENT_TYPE_SCROLL_RIGHT) {
        col_start++;
        wndmgr_mark_all_windows_dirty(window);
    }

    extra_data->row_start = row_start;
    extra_data->col_start = col_start;

    return 0;
}

static int8_t wnd_editor_on_destroy(const window_event_t* event) {
    window_t* window                    = event->window;
    wnd_editor_extra_data_t* extra_data = window->extra_data;

    if(!extra_data->is_text_readonly) {
        memory_free((void*)extra_data->text);
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t windowmanager_create_and_show_editor_window(const char16_t* title, const char16_t* text,
                                                   boolean_t is_text_readonly, boolean_t is_editor_readonly) {
    window_top_window_t top_window = windowmanager_create_top_window(title, true);

    if(!top_window.main_window || !top_window.inside_window) {
        return -1;
    }

    window_t* window = top_window.inside_window;

    wnd_editor_extra_data_t* extra_data = memory_malloc(sizeof(wnd_editor_extra_data_t));

    if(!extra_data) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    extra_data->ruler_window       = NULL;
    extra_data->editor_window      = NULL;
    extra_data->text               = text;
    extra_data->is_text_readonly   = is_text_readonly;
    extra_data->is_editor_readonly = is_editor_readonly;
    extra_data->row_start          = 0;
    extra_data->col_start          = 0;

    window->extra_data              = extra_data;
    window->extra_data_is_allocated = true;
    window->on_predraw              = wnd_editor_on_predraw;
    window->on_scroll               = wnd_editor_on_scroll;
    window->on_destroy              = wnd_editor_on_destroy;

    windowmanager_insert_and_set_current_window(top_window.main_window);


    return 0;
}
#pragma GCC diagnostic pop
