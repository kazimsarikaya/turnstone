/**
 * @file wnd_gfx.64.c
 * @brief Window manager graphics implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_gfx.h>
#include <graphics/screen.h>
#include <graphics/text_cursor.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

extern pixel_t* VIDEO_BASE_ADDRESS;

int8_t wnd_gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, color_t color) {
    UNUSED(buffer);
    UNUSED(area_width);

    SCREEN_CLEAR_AREA(rect.x, rect.y, rect.width, rect.height, color);

    return 0;
}

color_t wnd_gfx_blend_colors(color_t fg_color, color_t bg_color) {
    if(bg_color.color == 0) {
        return fg_color;
    }

    float32_t alpha1 = fg_color.alpha;
    float32_t red1 = fg_color.red;
    float32_t green1 = fg_color.green;
    float32_t blue1 = fg_color.blue;

    float32_t alpha2 = bg_color.alpha;
    float32_t red2 = bg_color.red;
    float32_t green2 = bg_color.green;
    float32_t blue2 = bg_color.blue;

    float32_t r = (alpha1 / 255) * red1;
    float32_t g = (alpha1 / 255) * green1;
    float32_t b = (alpha1 / 255) * blue1;

    r = r + (((255 - alpha1) / 255) * (alpha2 / 255)) * red2;
    g = g + (((255 - alpha1) / 255) * (alpha2 / 255)) * green2;
    b = b + (((255 - alpha1) / 255) * (alpha2 / 255)) * blue2;

    float32_t new_alpha = alpha1 + ((255 - alpha1) / 255) * alpha2;

    uint32_t ir = (uint32_t)r;
    uint32_t ig = (uint32_t)g;
    uint32_t ib = (uint32_t)b;
    uint32_t ia = (uint32_t)new_alpha;

    color_t fg_color_over_bg_color = {.color = (ia << 24) | (ir << 16) | (ig << 8) | (ib << 0)};

    return fg_color_over_bg_color;
}

boolean_t windowmanager_draw_window(window_t* window) {
    if(window == NULL) {
        return false;
    }

    if(!window->is_visible) {
        return false;
    }

    screen_info_t screen_info = screen_get_info();

    boolean_t flush_needed = false;

    if(window->is_dirty) {
        flush_needed = true;
        text_cursor_hide();

        if(window->on_redraw) {
            window_event_t event = {.type = WINDOW_EVENT_TYPE_REDRAW, .window = window};
            window->on_redraw(&event);
        }

        wnd_gfx_draw_rectangle(window->buffer, screen_info.pixels_per_scanline, window->rect, window->background_color);

        windowmanager_print_text(window, 0, 0, window->text);

        window->is_dirty = false;
    }

    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);

        child->is_dirty = flush_needed | child->is_dirty;

        // boolean_t child_flush_needed = windowmanager_draw_window(child);

        flush_needed = flush_needed | windowmanager_draw_window(child);
    }

    return flush_needed;
}

void windowmanager_print_glyph(const window_t* window, uint32_t x, uint32_t y, char16_t wc) {
    screen_info_t screen_info = screen_get_info();

    SCREEN_PRINT_GLYPH_WITH_STRIDE(wc,
                                   window->foreground_color, window->background_color,
                                   VIDEO_BASE_ADDRESS,
                                   x, y,
                                   screen_info.pixels_per_scanline);

}

void windowmanager_print_text(const window_t* window, uint32_t x, uint32_t y, const char_t* text) {
    if(window == NULL) {
        return;
    }

    if(text == NULL) {
        return;
    }

    screen_info_t screen_info = screen_get_info();

    if(window->rect.x + (int64_t)x >= screen_info.width || window->rect.y + (int64_t)y >= screen_info.height) {
        return;
    }

    uint32_t font_width = 0, font_height = 0;

    font_get_font_dimension(&font_width, &font_height);

    uint32_t abs_x = (window->rect.x + x) / font_width;
    uint32_t abs_y = (window->rect.y + y) / font_height;

    uint32_t cur_x = abs_x;
    uint32_t cur_y = abs_y;

    uint32_t max_cur_x = (window->rect.x + window->rect.width) / font_width;
    uint32_t max_cur_y = (window->rect.y + window->rect.height) / font_height;

    if(cur_x >= max_cur_x || cur_y >= max_cur_y) {
        return;
    }

    int64_t i = 0;

    while(text[i]) {
        char16_t wc = font_get_wc(text + i, &i);

        if(wc == '\n') {
            cur_y += 1;

            if(cur_y >= max_cur_y) {
                break;
            }

            cur_x = abs_x;
        } else {
            windowmanager_print_glyph(window, cur_x, cur_y, wc);
            cur_x += 1;

            if(cur_x >= max_cur_x && text[i + 1] && text[i + 1] != '\n') {
                cur_y += 1;

                if(cur_y >= max_cur_y) {
                    break;
                }

                cur_x = abs_x;
            }
        }

        i++;
    }
}

void windowmanager_clear_screen(window_t* window) {
    SCREEN_CLEAR_AREA(window->rect.x, window->rect.y, window->rect.width, window->rect.height, window->background_color);
}


