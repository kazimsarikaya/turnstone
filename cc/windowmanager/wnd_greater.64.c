/**
 * @file wnd_greater.64.c
 * @brief Window manager greater window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>

MODULE("turnstone.windowmanager");


extern char_t tos_logo_data_start;

window_t* windowmanager_create_greater_window(void) {
    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return NULL;
    }

    window->is_visible = true;
    window->is_dirty = true;

    char_t* windowmanager_turnstone_ascii_art = (char_t*)&tos_logo_data_start;

    rect_t rect = windowmanager_calc_text_rect(windowmanager_turnstone_ascii_art, 2000);
    rect.x = (VIDEO_GRAPHICS_WIDTH - rect.width) / 2;
    rect.y = (VIDEO_GRAPHICS_HEIGHT - rect.height) / 2;

    window_t* child = windowmanager_create_window(window,
                                                  windowmanager_turnstone_ascii_art,
                                                  rect,
                                                  (color_t){.color = 0x00000000},
                                                  (color_t){.color = 0xFFFF8822});

    if(child == NULL) {
        memory_free(window);
        return NULL;
    }

    child->is_visible = true;
    child->is_dirty = true;

    int32_t old_x = rect.x;
    int32_t old_y = rect.y;
    int32_t old_height = rect.height;

    rect = windowmanager_calc_text_rect("Press F2 to open panel", 2000);
    rect.x = old_x;
    rect.y = old_y + old_height + 4 * FONT_HEIGHT;

    child = windowmanager_create_window(window,
                                        "Press F2 to open panel",
                                        rect,
                                        (color_t){.color = 0x00000000},
                                        (color_t){.color = 0xFF00FF00});

    if(child == NULL) {
        memory_free(window);
        return NULL;
    }

    child->is_visible = true;
    child->is_dirty = true;

    return window;
}
