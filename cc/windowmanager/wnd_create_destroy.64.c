/**
 * @file wnd_create_destroy.64.c
 * @brief Window manager create and destroy window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <hashmap.h>
#include <buffer.h>
#include <graphics/screen.h>
#include <time.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

static int8_t wndmgr_footer_time_on_predraw(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    if(!window->sheets) {
        return -1;
    }

    if(list_size(window->sheets) != 1) {
        return -1;
    }

    window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, 0);

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = strprintf("%02d:%02d:%02d %04d-%02d-%02d",
                                 tp.hours, tp.minutes, tp.seconds,
                                 tp.year, tp.month, tp.day);

    memory_free(sheet->text);

    sheet->text = time_str;

    return 0;
}

static int8_t wndmgr_footer_fps_on_predraw(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    if(!window->sheets) {
        return -1;
    }

    if(list_size(window->sheets) != 1) {
        return -1;
    }

    window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, 0);

    char_t* fps_str = strprintf("FPS: %.02f", (wndmgr->previous_render_time) ? (1000000.0f / (float32_t)wndmgr->previous_render_time) : 1000000.0f);

    memory_free(sheet->text);

    sheet->text = fps_str;

    return 0;
}

static int8_t wndmgr_create_footer(window_t* parent) {
    if(parent == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    rect_t rect = {0, parent->owner_rect.height - wndmgr->font_height, parent->owner_rect.width, wndmgr->font_height};

    window_t* footer = windowmanager_create_window(parent, rect, .background_color = (color_t){.color = 0xFF282828});

    if(footer == NULL) {
        return -1;
    }

    timeparsed_t tp = {0};

    timeparsed(&tp);

    char_t* time_str = strprintf("%02d:%02d:%02d %04d-%02d-%02d",
                                 tp.hours, tp.minutes, tp.seconds,
                                 tp.year, tp.month, tp.day);

    rect = wndmgr_calc_text_rect(time_str, wndmgr->screen_width);

    rect.x = parent->owner_rect.width - rect.width - wndmgr->font_width;

    window_t* time_wnd = windowmanager_create_window(footer, rect,
                                                     (color_t){.color = 0xFF2288FF},
                                                     .text = time_str);

    memory_free(time_str);

    if(time_wnd == NULL) {
        windowmanager_destroy_window(footer);
        return -1;
    }

    time_wnd->on_predraw = wndmgr_footer_time_on_predraw;
    wndmgr_mark_window_sheets_always_redrawn(time_wnd, true);

    uint32_t fps_wnd_width = wndmgr->font_width * 20;

    rect = (rect_t){wndmgr->font_width, 0, fps_wnd_width, wndmgr->font_height};

    char_t* fps_str = strprintf("FPS: %.02f", (wndmgr->previous_render_time) ? (1000000.0f / (float32_t)wndmgr->previous_render_time) : 1000000.0f);

    window_t* fps_wnd = windowmanager_create_window(footer, rect,
                                                    (color_t){.color = 0xFF22FF22},
                                                    .text = fps_str);

    memory_free(fps_str);

    if(fps_wnd == NULL) {
        windowmanager_destroy_window(footer);
        return -1;
    }

    fps_wnd->on_predraw = wndmgr_footer_fps_on_predraw;
    wndmgr_mark_window_sheets_always_redrawn(fps_wnd, true);

    return 0;
}

window_t* windowmanager_command_input_window(window_t* parent, rect_t pos,
                                             const char_t* label_text,
                                             const char_t* input_text,
                                             const char_t* input_text_id,
                                             const char_t* tooltip_text) {

    windowmanager_t* wndmgr = windowmanager_get_instance();
    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;
    uint32_t screen_width = wndmgr->screen_width;


    window_t* option_input_row = windowmanager_create_window(parent,
                                                             (rect_t){font_width,
                                                                      pos.y,
                                                                      screen_width - font_width,
                                                                      font_height},
                                                             (color_t){.color = 0xFF00FF00});

    if(option_input_row == NULL) {
        return NULL;
    }

    char_t* input_label_text = strprintf("%s ==> ", label_text);

    rect_t rect = wndmgr_calc_text_rect(input_label_text, screen_width);

    window_t* option_input_label = windowmanager_create_window(option_input_row,
                                                               rect,
                                                               (color_t){.color = 0xFF00FF00},
                                                               .text = input_label_text);

    if(option_input_label == NULL) {
        windowmanager_destroy_window(option_input_row);
        return NULL;
    }

    char_t* wnd_input_text = strdup(input_text);

    rect = wndmgr_calc_text_rect(input_text, screen_width);

    rect.x = option_input_label->owner_rect.width + 2 * font_width;

    window_t* option_input_text = windowmanager_create_window(option_input_row,
                                                              rect,
                                                              (color_t){.color = 0xFFFF0000},
                                                              .text = wnd_input_text);

    if(option_input_text == NULL) {
        return NULL;
    }

    wndmgr_set_window_writable(option_input_text, true);
    option_input_text->input_length = strlen(input_text);
    option_input_text->input_id     = input_text_id;

    if(tooltip_text == NULL) {
        return option_input_row;
    }

    rect = wndmgr_calc_text_rect(tooltip_text, screen_width);

    rect.x = option_input_text->owner_rect.x + option_input_text->owner_rect.width  + 2 * font_width;

    char_t* tooltip_text_str = strndup(tooltip_text, rect.width);

    window_t* option_input_tooltip = windowmanager_create_window(option_input_row,
                                                                 rect,
                                                                 (color_t){.color = 0xFF00FF00},
                                                                 .text = tooltip_text_str);

    if(option_input_tooltip == NULL) {
        windowmanager_destroy_window(option_input_row);
        return NULL;
    }

    return option_input_row;
}

static int8_t wndmgr_window_sheet_destroyer(memory_heap_t* heap, void* item){
    window_sheet_t* sheet = (window_sheet_t*)item;

    if(sheet == NULL) {
        return -1;
    }

    memory_free_ext(heap, sheet->text);
    memory_free_ext(heap, sheet);

    return 0;
}

window_top_window_t windowmanager_create_top_window(const char_t* title, boolean_t has_command_input) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = memory_malloc(sizeof(window_t));

    if(!window) {
        return (window_top_window_t){0};
    }

    window->sheets = list_create_list();

    if(!window->sheets) {
        memory_free(window);
        return (window_top_window_t){0};
    }

    for(uint32_t x = 0; x < wndmgr->screen_width; x += wndmgr->sheet_tile_size) {
        for(uint32_t y = 0; y < wndmgr->screen_height; y += wndmgr->sheet_tile_size) {
            window_sheet_t* sheet = memory_malloc(sizeof(window_sheet_t));

            if(!sheet) {
                list_destroy_with_type(window->sheets, LIST_DESTROY_WITH_DATA, wndmgr_window_sheet_destroyer);
                memory_free(window);
                return (window_top_window_t){0};
            }

            uint64_t max_sheet_width  = MIN(wndmgr->sheet_tile_size, wndmgr->screen_width - x);
            uint64_t max_sheet_height = MIN(wndmgr->sheet_tile_size, wndmgr->screen_height - y);


            sheet->rect             = (rect_t){x, y, max_sheet_width, max_sheet_height};
            sheet->absolute_rect    = sheet->rect;
            sheet->is_dirty         = true;
            sheet->background_color = (color_t){.color = 0xFF000000};
            sheet->foreground_color = (color_t){.color = 0xFFFFFFFF};

            if(list_list_insert(window->sheets, sheet) == -1ULL) {
                memory_free(sheet);
                list_destroy_with_type(window->sheets, LIST_DESTROY_WITH_DATA, wndmgr_window_sheet_destroyer);
                memory_free(window);
                return (window_top_window_t){0};
            }
        }
    }

    window->wndmgr              = wndmgr;
    window->id                  = wndmgr->next_window_id++;
    window->owner_rect.x        = 0;
    window->owner_rect.y        = 0;
    window->owner_rect.width    = wndmgr->screen_width;
    window->owner_rect.height   = wndmgr->screen_height;
    window->owner_absolute_rect = window->owner_rect;

    rect_t title_rect = {0, 0, 0, 0};

    if(title) {
        char_t* title_str = strdup(title);

        title_rect   = wndmgr_calc_text_rect(title_str, wndmgr->screen_width);
        title_rect.x = (window->owner_rect.width - title_rect.width) / 2;
        title_rect.y = wndmgr->font_height;


        window_t* title_window = windowmanager_create_window(window,
                                                             title_rect,
                                                             (color_t){.color = 0xFF2288FF},
                                                             .text = title_str);

        if(title_window == NULL) {
            windowmanager_destroy_window(window);
            return (window_top_window_t){0};
        }

        title_rect.y = title_window->owner_rect.y + title_window->owner_rect.height;

    } else {
        title_rect.y = wndmgr->font_height;
    }

    title_rect.x = 0;

    if(has_command_input) {
        window_t* command_input_row = windowmanager_command_input_window(window, title_rect,
                                                                         WINDOWMANAGER_COMMAND_TEXT,
                                                                         WINDOWMANAGER_COMMAND_INPUT_TEXT,
                                                                         "option",
                                                                         NULL);

        if(!command_input_row) {
            windowmanager_destroy_window(window);
            return (window_top_window_t){0};
        }

        title_rect.y = command_input_row->owner_rect.y + command_input_row->owner_rect.height + wndmgr->font_height;
    } else {
        title_rect.y += wndmgr->font_height;
    }

    title_rect.x      = 0;
    title_rect.y     += wndmgr->font_height;
    title_rect.width  = wndmgr->screen_width;
    title_rect.height = wndmgr->screen_height - title_rect.y - wndmgr->font_height;

    window_t* inside_window = windowmanager_create_window(window, title_rect);

    if(!inside_window) {
        windowmanager_destroy_window(window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create inside window for top window");
        return (window_top_window_t){0};
    }


    if(wndmgr_create_footer(window) != 0) {
        windowmanager_destroy_window(window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create footer for top window");
        return (window_top_window_t){0};

    }

    return (window_top_window_t){
               .main_window   = window,
               .inside_window = inside_window,
    };
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
window_t* windowmanager_create_window_internal(windowmanager_create_window_args_t args) {
    if(!args.parent) {
        return NULL;
    }

    if(list_size(args.parent->sheets) == 0) {
        return NULL;
    }

    windowmanager_t* wndmgr = args.parent->wndmgr;

    window_sheet_t* first_sheet = (window_sheet_t*)list_get_data_at_position(args.parent->sheets, 0);

    if(args.background_color.color == 0x00000000) {
        args.background_color.color = first_sheet->background_color.color;
    }

    if(args.foreground_color.color == 0x00000000) {
        args.foreground_color.color = first_sheet->foreground_color.color;
    }

    window_t* window = memory_malloc(sizeof(window_t));

    if(!window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for window");
        return NULL;
    }

    window->sheets = list_create_list();

    if(!window->sheets) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create sheets list for window");
        memory_free(window);
        return NULL;
    }

    window_sheet_t* sheet = memory_malloc(sizeof(window_sheet_t));

    if(!sheet) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for window sheet");
        list_destroy(window->sheets);
        memory_free(window);
        return NULL;
    }

    boolean_t should_be_single_sheet = args.is_single_sheet || (args.text != NULL);

    if(should_be_single_sheet && list_list_insert(window->sheets, sheet) == -1ULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to insert sheet into window sheets list");
        memory_free(sheet);
        list_destroy(window->sheets);
        memory_free(window);
        return NULL;
    }

    sheet->rect          = args.rect;
    sheet->absolute_rect = (rect_t){
        .x      = args.parent->owner_absolute_rect.x + args.rect.x,
        .y      = args.parent->owner_absolute_rect.y + args.rect.y,
        .width  = args.rect.width,
        .height = args.rect.height,
    };
    sheet->is_dirty         = true;
    sheet->background_color = args.background_color;
    sheet->foreground_color = args.foreground_color;
    sheet->text             = strdup(args.text);

    args.parent->sheets = wndmgr_substract_sheets(args.parent->sheets, sheet);

    if(!args.parent->sheets) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to substruct sheet from parent sheets list");
        memory_free(sheet);
        list_destroy(window->sheets);
        memory_free(window);
        return NULL;
    }

    window->wndmgr              = wndmgr;
    window->id                  = wndmgr->next_window_id++;
    window->owner_rect          = sheet->rect;
    window->owner_absolute_rect = sheet->absolute_rect;

    window->parent = args.parent;

    if(args.parent->children == NULL) {
        args.parent->children = list_create_queue();
    }

    list_queue_push(args.parent->children, window);

    if(!should_be_single_sheet) {
        if(sheet->rect.width <= wndmgr->sheet_tile_size && sheet->rect.height <= wndmgr->sheet_tile_size &&
           list_list_insert(window->sheets, sheet) == -1ULL) {
            PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to insert sheet into window sheets list");
            memory_free(sheet);
            list_destroy(window->sheets);
            memory_free(window);
            return NULL;
        } else {
            // divide sheet into tiles and insert into window sheets list
            for(uint32_t x = args.rect.x; x < args.rect.x + args.rect.width; x += wndmgr->sheet_tile_size) {
                for(uint32_t y = args.rect.y; y < args.rect.y + args.rect.height; y += wndmgr->sheet_tile_size) {
                    uint64_t max_sheet_width  = MIN(wndmgr->sheet_tile_size, args.rect.x + args.rect.width - x);
                    uint64_t max_sheet_height = MIN(wndmgr->sheet_tile_size, args.rect.y + args.rect.height - y);

                    window_sheet_t* tile_sheet = memory_malloc(sizeof(window_sheet_t));

                    if(!tile_sheet) {
                        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for window sheet tile");
                        list_destroy(window->sheets);
                        memory_free(window);
                        return NULL;
                    }

                    tile_sheet->rect          = (rect_t){x, y, max_sheet_width, max_sheet_height};
                    tile_sheet->absolute_rect = (rect_t){
                        .x      = args.parent->owner_absolute_rect.x + x,
                        .y      = args.parent->owner_absolute_rect.y + y,
                        .width  = max_sheet_width,
                        .height = max_sheet_height,
                    };
                    tile_sheet->is_dirty         = true;
                    tile_sheet->background_color = args.background_color;
                    tile_sheet->foreground_color = args.foreground_color;

                    if(list_list_insert(window->sheets, tile_sheet) == -1ULL) {
                        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to insert sheet tile into window sheets list");
                        memory_free(tile_sheet);
                        list_destroy_with_type(window->sheets, LIST_DESTROY_WITH_DATA, wndmgr_window_sheet_destroyer);
                        memory_free(window);
                        return NULL;
                    }
                }
            }

            memory_free(sheet);
        }
    }

    return window;
}

void wndmgr_make_window_single_sheet(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(list_size(window->sheets) <= 1) {
        return;
    }

    window_sheet_t* single_sheet = memory_malloc(sizeof(window_sheet_t));

    if(single_sheet == NULL) {
        return;
    }

    single_sheet->rect             = window->owner_rect;
    single_sheet->absolute_rect    = window->owner_absolute_rect;
    single_sheet->is_dirty         = true;
    single_sheet->background_color = (color_t){.color = 0xFF000000};
    single_sheet->foreground_color = (color_t){.color = 0xFFFFFFFF};
    single_sheet->text             = NULL;

    list_destroy_with_type(window->sheets, LIST_DESTROY_WITH_DATA, wndmgr_window_sheet_destroyer);

    window->sheets = list_create_list();

    if(!window->sheets) {
        memory_free(single_sheet);
        return;
    }

    if(list_list_insert(window->sheets, single_sheet) == -1ULL) {
        memory_free(single_sheet);
        list_destroy(window->sheets);
        window->sheets = NULL;
        return;
    }
}

#pragma GCC diagnostic pop

void windowmanager_insert_and_set_current_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* next = wndmgr->current_window->next;

    window->next = next;

    if(next != NULL) {
        next->prev = window;
    }

    window->prev                 = wndmgr->current_window;
    wndmgr->current_window->next = window;

    wndmgr->current_window = window;
}

void windowmanager_destroy_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->children != NULL) {
        for (size_t i = 0; i < list_size(window->children); i++) {
            window_t* child = (window_t*)list_get_data_at_position(window->children, i);
            windowmanager_destroy_window(child);
        }

        list_destroy(window->children);
    }

    if(window->sheets != NULL) {
        for(size_t i = 0; i < list_size(window->sheets); i++) {
            window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);

            if(sheet->text) {
                memory_free(sheet->text);
            }

            memory_free(sheet);
        }

        list_destroy(window->sheets);
    }

    if(window->extra_data_is_allocated && window->extra_data != NULL) {
        memory_free(window->extra_data);
    }

    memory_free(window);
}

void windowmanager_destroy_child_window(window_t* window, window_t* child) {
    if(window == NULL || child == NULL) {
        return;
    }

    if(window->children == NULL) {
        return;
    }

    list_list_delete(window->children, child);

    windowmanager_destroy_window(child);
}

void windowmanager_destroy_all_child_windows(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->children == NULL) {
        return;
    }

    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);
        windowmanager_destroy_window(child);
    }

    list_destroy(window->children);

    window->children = NULL;
}

void windowmanager_remove_and_set_current_window(window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->prev == NULL) {
        return;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* next = wndmgr->current_window->next;
    window_t* prev = window->prev;

    prev->next = next;

    if(next != NULL) {
        next->prev = prev;
    }

    wndmgr->current_window = prev;

    wndmgr_mark_all_windows_dirty(wndmgr->current_window);

    windowmanager_destroy_window(window);
}

typedef struct wndmgr_alert_window_data {
    window_event_f on_enter;
    window_t*      self;
    void*          extra_data;
} wndmgr_alert_window_data_t;

static int8_t wndmgr_alert_window_on_enter(const window_event_t* event) {
    if(event == NULL) {
        return -1;
    }

    windowmanager_t* wndmgr = windowmanager_get_instance();

    wndmgr_alert_window_data_t* alert_window_data = (wndmgr_alert_window_data_t*)wndmgr->current_window->extra_data;
    window_t* alert_window                        = alert_window_data->self;

    if(alert_window == NULL) {
        return -1;
    }

    list_list_delete(wndmgr->current_window->children, alert_window);

    wndmgr->current_window->on_enter   = alert_window_data->on_enter;
    wndmgr->current_window->extra_data = alert_window_data->extra_data;
    wndmgr->has_alert                  = false;

    wndmgr_mark_all_windows_dirty(wndmgr->current_window);

    windowmanager_destroy_window(alert_window);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void windowmanager_create_and_show_alert_window(windowmanager_alert_window_type_t type, const char_t* text) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    rect_t rect = wndmgr_calc_text_rect(text, 400);

    rect_t alert_rect = {0, 0, rect.width + wndmgr->font_width * 4, rect.height + wndmgr->font_height * 4};

    alert_rect.x = (wndmgr->screen_height - alert_rect.width) / 2;
    alert_rect.y = (wndmgr->screen_height - alert_rect.height) / 2;

    int32_t linecharcount = alert_rect.width / wndmgr->font_width;
    int32_t linecount     = alert_rect.height / wndmgr->font_height;

    buffer_t* text_buffer = buffer_new_with_capacity(NULL, linecharcount * linecount);

    if(text_buffer == NULL) {
        return;
    }

    for(int32_t i = 0; i < linecharcount; i++) {
        buffer_append_byte(text_buffer, '*');
    }

    for(int32_t i = 0; i < linecount - 2; i++) {
        buffer_append_bytes(text_buffer, (uint8_t*)"* ", 2);

        for(int32_t j = 0; j < linecharcount - 4; j++) {
            buffer_append_byte(text_buffer, ' ');
        }

        buffer_append_bytes(text_buffer, (uint8_t*)" *", 2);
    }

    for(int32_t i = 0; i < linecharcount; i++) {
        buffer_append_byte(text_buffer, '*');
    }

    char_t* frame_text = (char_t*)buffer_get_all_bytes_and_destroy(text_buffer, NULL);

    color_t foreground_color;
    color_t background_color = {.color = 0x00000000};

    if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_INFO) {
        foreground_color.color = 0xFF2288FF;
    } else if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_WARNING) {
        foreground_color.color = 0xFFFF8800;
    } else if(type == WINDOWMANAGER_ALERT_WINDOW_TYPE_ERROR) {
        foreground_color.color = 0xFFFF0000;
    } else {
        foreground_color.color = 0xFFFFFFFF;
    }

    window_t* alert_window = windowmanager_create_window(wndmgr->current_window, alert_rect,
                                                         foreground_color,
                                                         background_color,
                                                         frame_text);

    memory_free(frame_text);

    if(alert_window == NULL) {
        return;
    }

    rect.x = 2 * wndmgr->font_width;
    rect.y = 2 * wndmgr->font_height;

    window_t* text_window = windowmanager_create_window(alert_window, rect,
                                                        foreground_color,
                                                        background_color,
                                                        text);

    if(text_window == NULL) {
        windowmanager_destroy_window(alert_window);
        return;
    }

    wndmgr_alert_window_data_t* alert_window_data = memory_malloc(sizeof(wndmgr_alert_window_data_t));

    if(alert_window_data == NULL) {
        windowmanager_destroy_window(alert_window);
        return;
    }

    alert_window_data->self       = alert_window;
    alert_window_data->on_enter   = wndmgr->current_window->on_enter;
    alert_window_data->extra_data = wndmgr->current_window->extra_data;

    wndmgr->current_window->on_enter   = wndmgr_alert_window_on_enter;
    wndmgr->current_window->extra_data = alert_window_data;
    wndmgr->has_alert                  = true;

    wndmgr_mark_all_windows_dirty(wndmgr->current_window);
}
#pragma GCC diagnostic pop
