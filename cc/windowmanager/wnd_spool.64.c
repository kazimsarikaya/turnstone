/**
 * @file wnd_spool.64.c
 * @brief Window Manager Spool Browser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_spool_browser.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <windowmanager/wnd_options.h>
#include <windowmanager/wnd_editor.h>
#include <strings.h>
#include <spool.h>
#include <argumentparser.h>
#include <graphics/screen.h>
#include <logging.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

static int8_t wndmgr_spool_item_on_enter(const window_event_t* event) {
    if(!event) {
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        return -1;
    }

    list_t* inputs = windowmanager_get_input_values(window);

    if(!inputs) {
        return -1;
    }

    if(list_size(inputs) == 0) {
        list_destroy(inputs);
        return 0;
    }

    for(size_t i = 0; i < list_size(inputs); i++) {
        const window_input_value_t* input = list_get_data_at_position(inputs, i);

        if(!input) {
            continue;
        }

        if(strcmp(input->id, "buffer") == 0) {
            const buffer_t* buffer = (const buffer_t*)input->extra_data;

            if(!buffer) {
                continue;
            }

            char_t* buffer_text = input->value;

            if(!buffer_text) {
                continue;
            }

            if(strlen(buffer_text) == 0) {
                continue;
            }

            if(strcmp(buffer_text, "s") == 0) {
                const char_t* buffer_data = (const char_t*)buffer_get_view_at_position(buffer, 0, buffer_get_length(buffer));
                windowmanager_create_and_show_editor_window("Spool Data", buffer_data, true);
                break;
            }
        }
    }

    windowmanager_destroy_inputs(inputs);

    return 0;
}

typedef struct sposl_item_window_extra_data {
    size_t          buffer_id;
    const buffer_t* buffer;
} spool_item_window_extra_data_t;

static int8_t wndmgr_spool_item_on_redraw(const window_event_t* event) {
    if(!event) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window event is NULL");
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window is NULL");
        return -1;
    }

    const spool_item_window_extra_data_t* sied = (const spool_item_window_extra_data_t*)window->extra_data;

    if(!sied) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window extra data of spool item is NULL");
        return -1;
    }

    char_t* spool_text = strprintf("% 12i% 15lli",
                                   sied->buffer_id,
                                   buffer_get_length(sied->buffer));

    memory_free(window->text);

    ((window_t*)window)->text = spool_text;

    return 0;
}

static int8_t windowmanager_create_and_show_spool_item_window(spool_item_t* spool_item){
    screen_info_t screen_info = screen_get_info();

    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    char_t* title_str = strprintf("tOS Spool Item %s Details", spool_get_name(spool_item));

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

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return -1;
    }

    window_t* wnd_header = windowmanager_create_window(window,
                                                       NULL,
                                                       (rect_t){0,
                                                                option_input_row->rect.y + option_input_row->rect.height + font_height,
                                                                screen_info.width,
                                                                font_height},
                                                       (color_t){.color = 0x00000000},
                                                       (color_t){.color = 0xFFee9900});

    if(!wnd_header) {
        windowmanager_destroy_window(window);
        return -1;
    }

    char_t* header_text = strprintf("%- 5s% 12s% 15s",
                                    "Cmd", "Buffer Id", "Buffer Size");

    window_t* wnd_header_text = windowmanager_create_window(wnd_header,
                                                            header_text,
                                                            (rect_t){font_width, 0, screen_info.width - font_width, font_height},
                                                            (color_t){.color = 0x00000000},
                                                            (color_t){.color = 0xFFee9900});


    if(!wnd_header_text) {
        windowmanager_destroy_window(window);
        return -1;
    }

    size_t buf_cnt = spool_get_buffer_count(spool_item);

    int32_t left = font_width + 5 * font_width;
    int32_t top = wnd_header->rect.y + wnd_header->rect.height;

    for(size_t i = 0; i < buf_cnt; i++) {
        const buffer_t* buffer = spool_get_buffer(spool_item, i);

        window_t* wnd_spool_input = windowmanager_create_window(window,
                                                                strdup("_"),
                                                                (rect_t){font_width*2, top, font_width, font_height},
                                                                (color_t){.color = 0x00000000},
                                                                (color_t){.color = 0xFFF0000});

        if(!wnd_spool_input) {
            windowmanager_destroy_window(window);
            return -1;
        }

        wnd_spool_input->is_writable = true;
        wnd_spool_input->input_length = 1;
        wnd_spool_input->input_id = "buffer";
        wnd_spool_input->extra_data = (void*)buffer;

        char_t* spool_text = strprintf("% 12i% 15lli",
                                       i,
                                       buffer_get_length(buffer));

        window_t* wnd_spool = windowmanager_create_window(window,
                                                          spool_text,
                                                          (rect_t){left, top, screen_info.width - left, font_height},
                                                          (color_t){.color = 0x00000000},
                                                          (color_t){.color = 0xFF00FF00});

        if(!wnd_spool) {
            windowmanager_destroy_window(window);
            return -1;
        }

        spool_item_window_extra_data_t* sied = memory_malloc(sizeof(spool_item_window_extra_data_t));

        if(!sied) {
            windowmanager_destroy_window(window);
            return -1;
        }

        sied->buffer_id = i;
        sied->buffer = buffer;

        wnd_spool->on_redraw = wndmgr_spool_item_on_redraw;
        wnd_spool->extra_data = (void*)sied;
        wnd_spool->extra_data_is_allocated = true;

        top += font_height;
    }

    window->on_enter = wndmgr_spool_item_on_enter;

    windowmanager_insert_and_set_current_window(window);

    return 0;
}

static int8_t wndmgr_spool_browser_on_enter(const window_event_t* event) {
    if(!event) {
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        return -1;
    }

    list_t* inputs = windowmanager_get_input_values(window);

    if(!inputs) {
        return -1;
    }

    if(list_size(inputs) == 0) {
        list_destroy(inputs);
        return 0;
    }

    for(size_t i = 0; i < list_size(inputs); i++) {
        const window_input_value_t* input = list_get_data_at_position(inputs, i);

        if(!input) {
            continue;
        }

        if(strcmp(input->id, "spool") == 0) {
            const spool_item_t* spool = (const spool_item_t*)input->extra_data;

            if(!spool) {
                continue;
            }

            char_t* buffer = input->value;

            if(!buffer) {
                continue;
            }

            if(strlen(buffer) == 0) {
                continue;
            }

            if(strcmp(buffer, "s") == 0) {
                spool_item_t* si = input->extra_data;
                windowmanager_create_and_show_spool_item_window(si);
                break;
            }


        }
    }

    windowmanager_destroy_inputs(inputs);

    return 0;
}

static int8_t wndmgr_spool_browser_wnd_spool_on_redraw(const window_event_t* event) {
    if(!event) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window event is NULL");
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window is NULL");
        return -1;
    }

    const spool_item_t* spool = (const spool_item_t*)window->extra_data;

    if(!spool) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window extra data of spool item is NULL");
        return -1;
    }

    char_t* spool_text = strprintf("%- 30s% 15lli% 20lli",
                                   spool_get_name(spool),
                                   spool_get_buffer_count(spool),
                                   spool_get_total_buffer_size(spool));

    memory_free(window->text);

    ((window_t*)window)->text = spool_text;

    return 0;
}

int8_t windowmanager_create_and_show_spool_browser_window(void) {
    screen_info_t screen_info = screen_get_info();

    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    char_t* title_str = strdup("tOS Spool Browser");

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

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return -1;
    }

    window_t* wnd_header = windowmanager_create_window(window,
                                                       NULL,
                                                       (rect_t){0,
                                                                option_input_row->rect.y + option_input_row->rect.height + font_height,
                                                                screen_info.width,
                                                                font_height},
                                                       (color_t){.color = 0x00000000},
                                                       (color_t){.color = 0xFFee9900});

    if(!wnd_header) {
        windowmanager_destroy_window(window);
        return -1;
    }

    char_t* header_text = strprintf("%- 5s%- 30s% 15s% 20s",
                                    "Cmd", "Name", "Buffer Count", "Total Buffer Size");

    window_t* wnd_header_text = windowmanager_create_window(wnd_header,
                                                            header_text,
                                                            (rect_t){font_width, 0, screen_info.width - font_width, font_height},
                                                            (color_t){.color = 0x00000000},
                                                            (color_t){.color = 0xFFee9900});


    if(!wnd_header_text) {
        windowmanager_destroy_window(window);
        return -1;
    }

    int32_t left = font_width + 5 * font_width;
    int32_t top = wnd_header->rect.y + wnd_header->rect.height;

    list_t* spool_list = spool_get_all();

    for(size_t i = 0; i < list_size(spool_list); i++) {
        const spool_item_t* spool = list_get_data_at_position(spool_list, i);

        window_t* wnd_spool_input = windowmanager_create_window(window,
                                                                strdup("_"),
                                                                (rect_t){font_width*2, top, font_width, font_height},
                                                                (color_t){.color = 0x00000000},
                                                                (color_t){.color = 0xFFF0000});

        if(!wnd_spool_input) {
            windowmanager_destroy_window(window);
            return -1;
        }

        wnd_spool_input->is_writable = true;
        wnd_spool_input->input_length = 1;
        wnd_spool_input->input_id = "spool";
        wnd_spool_input->extra_data = (void*)spool;

        char_t* spool_text = strprintf("%- 30s% 15lli% 20lli",
                                       spool_get_name(spool),
                                       spool_get_buffer_count(spool),
                                       spool_get_total_buffer_size(spool));

        window_t* wnd_spool = windowmanager_create_window(window,
                                                          spool_text,
                                                          (rect_t){left, top, screen_info.width - left, font_height},
                                                          (color_t){.color = 0x00000000},
                                                          (color_t){.color = 0xFF00FF00});

        if(!wnd_spool) {
            windowmanager_destroy_window(window);
            return -1;
        }

        wnd_spool->on_redraw = wndmgr_spool_browser_wnd_spool_on_redraw;
        wnd_spool->extra_data = (void*)spool;

        top += font_height;
    }

    window->on_enter = wndmgr_spool_browser_on_enter;

    windowmanager_insert_and_set_current_window(window);

    return 0;
}
