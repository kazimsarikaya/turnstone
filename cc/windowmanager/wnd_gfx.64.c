/**
 * @file wnd_gfx.64.c
 * @brief Window manager graphics implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

int8_t gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, pixel_t color) {
    for (int32_t i = 0; i < rect.height; i++) {
        for (int32_t j = 0; j < rect.width; j++) {
            int32_t idx = (rect.y + i) * area_width + (rect.x + j);

            if(idx >= VIDEO_GRAPHICS_WIDTH * VIDEO_GRAPHICS_HEIGHT) {
                return -1;
            }

            buffer[idx] = color;
        }
    }

    return 0;
}

color_t gfx_blend_colors(color_t fg_color, color_t bg_color) {
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

    boolean_t flush_needed = false;

    if(window->is_dirty) {
        flush_needed = true;
        video_text_cursor_hide();

        gfx_draw_rectangle(window->buffer, VIDEO_PIXELS_PER_SCANLINE, window->rect, window->background_color.color);

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

