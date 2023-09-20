/**
 * @file windowmanager.64.c
 * @brief Window Manager implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <video.h>
#include <logging.h>
#include <memory.h>

MODULE("turnstone.user.programs.windowmanager");

typedef struct window_t {
    uint64_t      id;
    const char_t* title;
    uint32_t      x;
    uint32_t      y;
    uint32_t      width;
    uint32_t      height;
    pixel_t*      buffer;
} window_t;

boolean_t windowmanager_initialized = false;
uint64_t windowmanager_next_window_id = 0;

extern int32_t VIDEO_GRAPHICS_WIDTH;
extern int32_t VIDEO_GRAPHICS_HEIGHT;
extern uint32_t* VIDEO_BASE_ADDRESS;

window_t* windowmanager_create_window(const char_t* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
int8_t    gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, uint32_t x, uint32_t y, uint32_t width, uint32_t height, pixel_t color);
uint32_t  gfx_blend_colors(uint32_t color1, uint32_t color2);

#define GET_ALPHA(color) ((color >> 24) & 0xFF)
#define GET_RED(color)   ((color >> 16) & 0xFF)
#define GET_GREEN(color) ((color >> 8) & 0xFF)
#define GET_BLUE(color)  ((color >> 0) & 0xFF)

void video_text_print(const char_t* text);


uint32_t gfx_blend_colors(uint32_t fg_color, uint32_t bg_color) {
    if(bg_color == 0) {
        return fg_color;
    }

    float32_t alpha1 = GET_ALPHA(fg_color);
    float32_t red1 = GET_RED(fg_color);
    float32_t green1 = GET_GREEN(fg_color);
    float32_t blue1 = GET_BLUE(fg_color);

    float32_t alpha2 = GET_ALPHA(bg_color);
    float32_t red2 = GET_RED(bg_color);
    float32_t green2 = GET_GREEN(bg_color);
    float32_t blue2 = GET_BLUE(bg_color);

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

    uint32_t fg_color_over_bg_color = (ia << 24) | (ir << 16) | (ig << 8) | (ib << 0);

    return fg_color_over_bg_color;
}


window_t* windowmanager_create_window(const char_t* title, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    window->id     = windowmanager_next_window_id++;
    window->title  = title;
    window->x      = x;
    window->y      = y;
    window->width  = width;
    window->height = height;
    window->buffer = memory_malloc(width * height * sizeof(pixel_t));

    return window;
}

int8_t gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, uint32_t x, uint32_t y, uint32_t width, uint32_t height, pixel_t color) {
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            buffer[(y + i) * area_width + (x + j)] = color;
        }
    }

    return 0;
}

int8_t windowmanager_init(void) {
    window_t* window = windowmanager_create_window("Window Manager", 400, 400, 400, 400);

    if(window == NULL) {
        return -1;
    }

    gfx_draw_rectangle(window->buffer, window->width, 0, 40, window->width, window->height - 40, 0x601B1B1B);
    gfx_draw_rectangle(window->buffer, window->width, 0, 0, window->width, 40, 0xCC304323);

    for (uint32_t i = 0; i < window->height; i++) {
        for (uint32_t j = 0; j < window->width; j++) {
            uint32_t color1 = window->buffer[i * window->width + j];
            uint32_t color2 = VIDEO_BASE_ADDRESS[(window->y + i) * VIDEO_GRAPHICS_WIDTH + (window->x + j)];
            uint32_t color = gfx_blend_colors(color1, color2);
            VIDEO_BASE_ADDRESS[(window->y + i) * VIDEO_GRAPHICS_WIDTH + (window->x + j)] = color;
        }
    }

    VIDEO_DISPLAY_FLUSH(0, 0, 0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT);

    memory_free(window->buffer);
    memory_free(window);

    return 0;
}
