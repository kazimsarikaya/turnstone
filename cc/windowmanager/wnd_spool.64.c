/**
 * @file wnd_spool.64.c
 * @brief Window Manager Spool Browser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
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

    list_t* inputs = wndmgr_get_input_values(window);

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

        if(wstrcmp(input->id, u"buffer") == 0) {
            const buffer_t* buffer = (const buffer_t*)input->extra_data;

            if(!buffer) {
                continue;
            }

            char16_t* buffer_text = input->value;

            if(!buffer_text) {
                continue;
            }

            if(wstrlen(buffer_text) == 0) {
                continue;
            }

            if(wstrcmp(buffer_text, u"s") == 0) {
                const char_t* c8_buffer_data = (const char_t*)(void*)buffer_get_view_at_position(buffer, 0, buffer_get_length(buffer));
                char16_t* buffer_data        = str_to_wstr(c8_buffer_data);
                windowmanager_create_and_show_editor_window(u"Spool Data", buffer_data, false, true);
                break;
            }
        }
    }

    wndmgr_destroy_inputs(inputs);

    return 0;
}

typedef struct sposl_item_window_extra_data {
    size_t          buffer_id;
    const buffer_t* buffer;
} spool_item_window_extra_data_t;

static int8_t wndmgr_spool_item_on_predraw(const window_event_t* event) {
    if(!event) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window event is NULL");
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window is NULL");
        return -1;
    }

    if(!window->sheets) {
        return -1;
    }

    if(list_size(window->sheets) != 1) {
        return -1;
    }

    window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, 0);

    if(!sheet) {
        return -1;
    }

    const spool_item_window_extra_data_t* sied = (const spool_item_window_extra_data_t*)window->extra_data;

    if(!sied) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window extra data of spool item is NULL");
        return -1;
    }

    char16_t* spool_text = wstrprintf("%12lli%15lli",
                                      sied->buffer_id,
                                      buffer_get_length(sied->buffer));

    memory_free(sheet->text);

    sheet->text = spool_text;

    return 0;
}

static int8_t windowmanager_create_and_show_spool_item_window(spool_item_t* spool_item){
    windowmanager_t* wndmgr = windowmanager_get_instance();

    char16_t* title_str = wstrprintf("tOS Spool Item %s Details", spool_get_name(spool_item));

    window_top_window_t top_window = windowmanager_create_top_window(title_str, true);

    memory_free(title_str);

    if(!top_window.main_window || !top_window.inside_window) {
        return -1;
    }

    window_t* window = top_window.inside_window;

    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;
    uint32_t screen_width = wndmgr->screen_width;


    char16_t* header_text = wstrprintf("%-5s%12s%15s",
                                       "Cmd", "Buffer Id", "Buffer Size");

    window_t* wnd_header_text = windowmanager_create_window(window,
                                                            (rect_t){font_width, 0, screen_width - font_width, font_height},
                                                            (color_t){.color = 0xFFee9900},
                                                            .text = header_text);


    if(!wnd_header_text) {
        windowmanager_destroy_window(top_window.main_window);
        return -1;
    }

    size_t buf_cnt = spool_get_buffer_count(spool_item);

    int32_t left = font_width + 5 * font_width;
    int32_t top  = wnd_header_text->owner_rect.y + wnd_header_text->owner_rect.height;

    for(size_t i = 0; i < buf_cnt; i++) {
        const buffer_t* buffer = spool_get_buffer(spool_item, i);

        window_t* wnd_spool_input = windowmanager_create_window(window,
                                                                (rect_t){font_width*2, top, font_width, font_height},
                                                                (color_t){.color = 0xFFF00000},
                                                                .text        = u" ",
                                                                .is_writable = true);

        if(!wnd_spool_input) {
            windowmanager_destroy_window(top_window.main_window);
            return -1;
        }

        wnd_spool_input->input_length = 1;
        wnd_spool_input->input_id     = u"buffer";
        wnd_spool_input->extra_data   = (void*)buffer;

        char16_t* spool_text = wstrprintf("%12lli%15lli",
                                          i,
                                          buffer_get_length(buffer));

        window_t* wnd_spool = windowmanager_create_window(window,
                                                          (rect_t){left, top, screen_width - left, font_height},
                                                          (color_t){.color = 0xFF00FF00},
                                                          .text = spool_text);

        memory_free(spool_text);

        if(!wnd_spool) {
            windowmanager_destroy_window(top_window.main_window);
            return -1;
        }

        spool_item_window_extra_data_t* sied = memory_malloc(sizeof(spool_item_window_extra_data_t));

        if(!sied) {
            windowmanager_destroy_window(top_window.main_window);
            return -1;
        }

        sied->buffer_id = i;
        sied->buffer    = buffer;

        wnd_spool->on_predraw              = wndmgr_spool_item_on_predraw;
        wnd_spool->extra_data              = (void*)sied;
        wnd_spool->extra_data_is_allocated = true;

        top += font_height;
    }

    window->on_enter = wndmgr_spool_item_on_enter;

    windowmanager_insert_and_set_current_window(top_window.main_window);

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

    list_t* inputs = wndmgr_get_input_values(window);

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

        if(wstrcmp(input->id, u"spool") == 0) {
            const spool_item_t* spool = (const spool_item_t*)input->extra_data;

            if(!spool) {
                continue;
            }

            char16_t* buffer = input->value;

            if(!buffer) {
                continue;
            }

            if(wstrlen(buffer) == 0) {
                continue;
            }

            if(wstrcmp(buffer, u"s") == 0) {
                spool_item_t* si = input->extra_data;
                windowmanager_create_and_show_spool_item_window(si);
                break;
            }


        }
    }

    wndmgr_destroy_inputs(inputs);

    return 0;
}

static int8_t wndmgr_spool_browser_wnd_spool_on_predraw(const window_event_t* event) {
    if(!event) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window event is NULL");
        return -1;
    }

    const window_t* window = event->window;

    if(!window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window is NULL");
        return -1;
    }

    if(!window->sheets) {
        return -1;
    }

    if(list_size(window->sheets) != 1) {
        return -1;
    }

    window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, 0);

    if(!sheet) {
        return -1;
    }

    const spool_item_t* spool = (const spool_item_t*)window->extra_data;

    if(!spool) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Window extra data of spool item is NULL");
        return -1;
    }

    char16_t* spool_text = wstrprintf("%-60s%15lli%20lli",
                                      spool_get_name(spool),
                                      spool_get_buffer_count(spool),
                                      spool_get_total_buffer_size(spool));

    memory_free(sheet->text);

    sheet->text = spool_text;

    return 0;
}

int8_t windowmanager_create_and_show_spool_browser_window(void) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_top_window_t top_window = windowmanager_create_top_window(u"tOS Spool Browser", true);

    if(!top_window.main_window || !top_window.inside_window) {
        return -1;
    }

    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;
    uint32_t screen_width = wndmgr->screen_width;


    window_t* window = top_window.inside_window;

    char16_t* header_text = wstrprintf("%-5s%-60s%15s%20s",
                                       "Cmd", "Name", "Buffer Count", "Total Buffer Size");

    window_t* wnd_header_text = windowmanager_create_window(window,
                                                            (rect_t){font_width, 0, screen_width - font_width, font_height},
                                                            (color_t){.color = 0XFFEE9900},
                                                            .text = header_text);

    memory_free(header_text);

    if(!wnd_header_text) {
        windowmanager_destroy_window(top_window.main_window);;
        return -1;
    }

    int32_t left = font_width + 5 * font_width;
    int32_t top  = wnd_header_text->owner_rect.y + wnd_header_text->owner_rect.height;

    list_t* spool_list = spool_get_all();

    for(size_t i = 0; i < list_size(spool_list); i++) {
        const spool_item_t* spool = list_get_data_at_position(spool_list, i);

        window_t* wnd_spool_input = windowmanager_create_window(window,
                                                                (rect_t){font_width*2, top, font_width, font_height},
                                                                (color_t){.color = 0xFFF00000},
                                                                .text        = u" ",
                                                                .is_writable = true);

        if(!wnd_spool_input) {
            windowmanager_destroy_window(top_window.main_window);
            return -1;
        }

        wnd_spool_input->input_length = 1;
        wnd_spool_input->input_id     = u"spool";
        wnd_spool_input->extra_data   = (void*)spool;

        char16_t* spool_text = wstrprintf("%-60s%15lli%20lli",
                                          spool_get_name(spool),
                                          spool_get_buffer_count(spool),
                                          spool_get_total_buffer_size(spool));

        window_t* wnd_spool = windowmanager_create_window(window,
                                                          (rect_t){left, top, screen_width - left, font_height},
                                                          (color_t){.color = 0xFF00FF00},
                                                          .text = spool_text);

        memory_free(spool_text);

        if(!wnd_spool) {
            windowmanager_destroy_window(top_window.main_window);
            return -1;
        }

        wnd_spool->on_predraw = wndmgr_spool_browser_wnd_spool_on_predraw;
        wnd_spool->extra_data = (void*)spool;

        top += font_height;
    }

    window->on_enter = wndmgr_spool_browser_on_enter;

    windowmanager_insert_and_set_current_window(top_window.main_window);

    return 0;
}
