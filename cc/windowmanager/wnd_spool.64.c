/**
 * @file wnd_spool.64.c
 * @brief Window Manager Spool Browser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <strings.h>
#include <spool.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

static int8_t wndmgr_spool_browser_on_enter(const window_t* window) {
    if(!window) {
        return -1;
    }

    video_text_print("Spool selected\n");


    return 0;
}

int8_t windowmanager_create_and_show_spool_browser_window(void) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return -1;
    }

    window->is_visible = true;
    window->is_dirty = true;

    char_t* title_str = strdup("tOS Spool Browser");

    rect_t rect = windowmanager_calc_text_rect(title_str, 2000);
    rect.x = (VIDEO_GRAPHICS_WIDTH - rect.width) / 2;
    rect.y = FONT_HEIGHT;


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

    window_t* option_input_row = windowmanager_add_option_window(window, title_window->rect);

    if(!option_input_row) {
        windowmanager_destroy_window(window);
        return -1;
    }

    window_t* wnd_header = windowmanager_create_window(window,
                                                       NULL,
                                                       (rect_t){0,
                                                                option_input_row->rect.y + option_input_row->rect.height + FONT_HEIGHT,
                                                                VIDEO_GRAPHICS_WIDTH,
                                                                FONT_HEIGHT},
                                                       (color_t){.color = 0x00000000},
                                                       (color_t){.color = 0xFFee9900});

    if(!wnd_header) {
        windowmanager_destroy_window(window);
        return -1;
    }

    wnd_header->is_visible = true;

    char_t* header_text = sprintf("%- 5s%- 30s% 15s% 20s",
                                  "Cmd", "Name", "Buffer Count", "Total Buffer Size");

    window_t* wnd_header_text = windowmanager_create_window(wnd_header,
                                                            header_text,
                                                            (rect_t){FONT_WIDTH, 0, VIDEO_GRAPHICS_WIDTH - FONT_WIDTH, FONT_HEIGHT},
                                                            (color_t){.color = 0x00000000},
                                                            (color_t){.color = 0xFFee9900});


    if(!wnd_header_text) {
        windowmanager_destroy_window(window);
        return -1;
    }

    wnd_header_text->is_visible = true;

    int32_t left = FONT_WIDTH + 5 * FONT_WIDTH;
    int32_t top = wnd_header->rect.y + wnd_header->rect.height;

    list_t* spool_list = spool_get_all();

    for(size_t i = 0; i < list_size(spool_list); i++) {
        const spool_item_t* spool = list_get_data_at_position(spool_list, i);

        window_t* wnd_spool_input = windowmanager_create_window(window,
                                                                strdup("__"),
                                                                (rect_t){FONT_WIDTH, top, FONT_WIDTH * 2, FONT_HEIGHT},
                                                                (color_t){.color = 0x00000000},
                                                                (color_t){.color = 0xFFF0000});

        if(!wnd_spool_input) {
            windowmanager_destroy_window(window);
            return -1;
        }

        wnd_spool_input->is_visible = true;
        wnd_spool_input->is_writable = true;
        wnd_spool_input->input_length = 2;
        wnd_spool_input->input_id = "spool";
        wnd_spool_input->extra_data = (void*)spool;

        char_t* spool_text = sprintf("%- 30s% 15d% 20d",
                                     spool_get_name(spool),
                                     spool_get_buffer_count(spool),
                                     spool_get_total_buffer_size(spool));

        window_t* wnd_spool = windowmanager_create_window(window,
                                                          spool_text,
                                                          (rect_t){left, top, VIDEO_GRAPHICS_WIDTH - left, FONT_HEIGHT},
                                                          (color_t){.color = 0x00000000},
                                                          (color_t){.color = 0xFF00FF00});

        if(!wnd_spool) {
            windowmanager_destroy_window(window);
            return -1;
        }

        wnd_spool->is_visible = true;

        top += FONT_HEIGHT;
    }

    window->on_enter = wndmgr_spool_browser_on_enter;

    windowmanager_insert_and_set_current_window(window);

    return 0;
}
