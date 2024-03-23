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
#include <utils.h>
#include <hashmap.h>
#include <cpu.h>
#include <cpu/task.h>
#include <utils.h>
#include <device/mouse.h>
#include <device/kbd.h>
#include <device/kbd_scancodes.h>

MODULE("turnstone.user.programs.windowmanager");

typedef struct window_t window_t;
typedef union color_t   color_t;
typedef struct rect_t   rect_t;

union color_t {
    struct {
        uint8_t alpha;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } __attribute__((packed));
    uint32_t color;
};

struct rect_t {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};

struct window_t {
    uint64_t      id;
    const char_t* text;
    boolean_t     is_dirty;
    boolean_t     is_visible;
    rect_t        rect;
    pixel_t*      buffer;
    color_t       background_color;
    color_t       foreground_color;
    window_t*     next;
    window_t*     prev;
    list_t*       children;
};

uint64_t windowmanager_next_window_id = 0;
window_t* windowmanager_current_window = NULL;
hashmap_t* windowmanager_windows = NULL;

extern int32_t VIDEO_GRAPHICS_WIDTH;
extern int32_t VIDEO_GRAPHICS_HEIGHT;
extern uint32_t* VIDEO_BASE_ADDRESS;
extern int32_t VIDEO_PIXELS_PER_SCANLINE;
extern uint8_t* FONT_ADDRESS;
extern int32_t FONT_WIDTH;
extern int32_t FONT_HEIGHT;
extern int32_t FONT_BYTES_PER_GLYPH;
extern int32_t FONT_CHARS_PER_LINE;
extern int32_t FONT_LINES_ON_SCREEN;
extern uint32_t FONT_MASK;
extern int32_t FONT_BYTES_PERLINE;

color_t gfx_blend_colors(color_t color1, color_t color2);

void video_text_print(const char_t* text);

static void windowmanager_print_glyph(const window_t* window, int32_t x, int32_t y, wchar_t wc) {
    uint8_t* glyph = FONT_ADDRESS + (wc * FONT_BYTES_PER_GLYPH);

    int32_t abs_x = window->rect.x + x;
    int32_t abs_y = window->rect.y + y;

    int32_t offs = (abs_y * VIDEO_PIXELS_PER_SCANLINE) + abs_x;

    int32_t tx, ty, line, mask;

    for(ty = 0; ty < FONT_HEIGHT; ty++) {
        line = offs;
        mask = FONT_MASK;

        uint32_t tmp = BYTE_SWAP32(*((uint32_t*)glyph));

        for(tx = 0; tx < FONT_WIDTH; tx++) {

            *((pixel_t*)(VIDEO_BASE_ADDRESS + line)) = tmp & mask ? window->foreground_color.color : window->background_color.color;

            mask >>= 1;
            line++;
        }

        glyph += FONT_BYTES_PERLINE;
        offs  += VIDEO_PIXELS_PER_SCANLINE;
    }
}

static void windowmanager_print_text(const window_t* window, int32_t x, int32_t y, const char_t* text) {
    if(window == NULL) {
        return;
    }

    if(text == NULL) {
        return;
    }

    int64_t i = 0;

    while(text[i]) {
        wchar_t wc = video_get_wc(text + i, &i);

        if(wc == '\n') {
            y += FONT_HEIGHT;
            x = 0;
        } else {
            windowmanager_print_glyph(window, x, y, wc);
            x += FONT_WIDTH;
        }

        text++;
    }
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

static void windowmanager_clear_screen(window_t* window) {
    for(int32_t i = 0; i < VIDEO_GRAPHICS_HEIGHT; i++) {
        for(int32_t j = 0; j < VIDEO_GRAPHICS_WIDTH; j++) {
            *((pixel_t*)(VIDEO_BASE_ADDRESS + (i * VIDEO_PIXELS_PER_SCANLINE) + j)) = window->background_color.color;
        }
    }
}


static window_t* windowmanager_create_top_window(void) {
    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    window->id     = windowmanager_next_window_id++;
    window->rect.x      = 0;
    window->rect.y      = 0;
    window->rect.width  = VIDEO_GRAPHICS_WIDTH;
    window->rect.height = VIDEO_GRAPHICS_HEIGHT;
    window->buffer = VIDEO_BASE_ADDRESS;
    window->background_color.color = 0x00000000;
    window->foreground_color.color = 0xFFFFFFFF;

    return window;
}


static window_t* windowmanager_create_window(window_t* parent, const char_t* text, rect_t rect, color_t background_color, color_t foreground_color) {
    window_t* window = memory_malloc(sizeof(window_t));

    if(window == NULL) {
        return NULL;
    }

    int32_t abs_x = parent->rect.x + rect.x;
    int32_t abs_y = parent->rect.y + rect.y;

    window->id     = windowmanager_next_window_id++;
    window->text  = text;
    window->rect   = (rect_t){abs_x, abs_y, rect.width, rect.height};
    window->buffer = VIDEO_BASE_ADDRESS + (abs_y * VIDEO_PIXELS_PER_SCANLINE) + abs_x;
    window->background_color = background_color;
    window->foreground_color = foreground_color;

    if(parent->children == NULL) {
        parent->children = list_create_queue();
    }

    list_queue_push(parent->children, window);

    return window;
}

static int8_t gfx_draw_rectangle(pixel_t* buffer, uint32_t area_width, rect_t rect, pixel_t color) {
    for (int32_t i = 0; i < rect.height; i++) {
        for (int32_t j = 0; j < rect.width; j++) {
            buffer[(rect.y + i) * area_width + (rect.x + j)] = color;
        }
    }

    return 0;
}

static rect_t windowmanager_calc_text_rect(const char_t* text, int32_t max_width) {
    if(text == NULL) {
        return (rect_t){0};
    }

    rect_t rect = {0};

    rect.x = 0;
    rect.y = 0;
    rect.width = 0;
    rect.height = 0;

    int32_t max_calc_width = 0;

    while(*text) {
        if(*text == '\n') {
            rect.height += FONT_HEIGHT;
            max_calc_width = MAX(max_calc_width, rect.width);
            rect.width = 0;
        } else {
            if(rect.width + FONT_WIDTH > max_width) {
                rect.height += FONT_HEIGHT;
                max_calc_width = MAX(max_calc_width, rect.width);
                rect.width = 0;
            } else {
                rect.width += FONT_WIDTH;
            }
        }

        text++;
    }

    rect.width = MAX(rect.width, max_calc_width);

    return rect;
}

extern char_t tos_logo_data_start;


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static window_t* windowmanager_create_main_window(void) {
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
#pragma GCC diagnostic pop

boolean_t windowmanager_draw_window(window_t* window);
boolean_t windowmanager_draw_window(window_t* window) {
    if(window == NULL) {
        video_text_print("window is NULL");
        return false;
    }

    if(!window->is_visible) {
        return false;
    }

    boolean_t flush_needed = false;

    if(window->is_dirty) {
        flush_needed = true;

        gfx_draw_rectangle(window->buffer, window->rect.width, window->rect, window->background_color.color);

        windowmanager_print_text(window, 0, 0, window->text);

        window->is_dirty = false;
    }

    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);

        child->is_dirty = flush_needed || child->is_dirty;

        boolean_t child_flush_needed = windowmanager_draw_window(child);

        flush_needed = flush_needed || child_flush_needed;
    }

    return flush_needed;
}

static uint32_t windowmanager_append_wchar_to_buffer(wchar_t src, char_t* dst, uint32_t dst_idx) {
    if(dst == NULL) {
        return NULL;
    }

    int64_t j = dst_idx;

    if(src >= 0x800) {
        dst[j++] = ((src >> 12) & 0xF) | 0xE0;
        dst[j++] = ((src >> 6) & 0x3F) | 0x80;
        dst[j++] = (src & 0x3F) | 0x80;
    } else if(src >= 0x80) {
        dst[j++] = ((src >> 6) & 0x1F) | 0xC0;
        dst[j++] = (src & 0x3F) | 0x80;
    } else {
        dst[j++] = src & 0x7F;
    }

    return j;
}

extern boolean_t windowmanager_initialized;
extern buffer_t* shell_buffer;
extern buffer_t* mouse_buffer;

static int8_t windowmanager_main(void) {
    windowmanager_current_window = windowmanager_create_main_window();
    windowmanager_windows = hashmap_integer(16);

    list_t* mq = list_create_queue();

    task_add_message_queue(mq);

    hashmap_put(windowmanager_windows, (void*)windowmanager_current_window->id, windowmanager_current_window);

    task_set_interruptible();

    shell_buffer = buffer_new_with_capacity(NULL, 4100);
    mouse_buffer = buffer_new_with_capacity(NULL, 4096);

    video_move_text_cursor(0, 0);
    windowmanager_clear_screen(windowmanager_current_window);
    VIDEO_DISPLAY_FLUSH(0, 0, 0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT);

    windowmanager_initialized = true;

    while(true) {
        // windowmanager_clear_screen(windowmanager_current_window);
        boolean_t flush_needed = windowmanager_draw_window(windowmanager_current_window);


        if(flush_needed) {
            VIDEO_DISPLAY_FLUSH(0, 0, 0, 0, VIDEO_GRAPHICS_WIDTH, VIDEO_GRAPHICS_HEIGHT);
        }

        while(list_size(mq) == 0) {
            task_set_message_waiting();
            task_yield();

            if(buffer_get_length(shell_buffer) == 0 && buffer_get_length(mouse_buffer) == 0) {
                continue;
            } else {
                break;
            }
        }

        uint64_t kbd_length = 0;
        uint32_t kbd_ev_cnt = 0;
        uint64_t mouse_length = 0;
        uint32_t mouse_ev_cnt = 0;

        kbd_report_t* kbd_data = (kbd_report_t*)buffer_get_all_bytes_and_reset(shell_buffer, &kbd_length);
        mouse_report_t* mouse_data = (mouse_report_t*)buffer_get_all_bytes_and_reset(mouse_buffer, &mouse_length);

        if(kbd_length == 0 && mouse_length == 0) {
            memory_free(kbd_data);
            memory_free(mouse_data);

            continue;
        }

        if(mouse_length) {
            mouse_ev_cnt = mouse_length / sizeof(mouse_report_t);

            if(VIDEO_MOVE_CURSOR) {
                VIDEO_MOVE_CURSOR(mouse_data[mouse_ev_cnt - 1].x, mouse_data[mouse_ev_cnt - 1].y);
            }
        }

        memory_free(mouse_data);

        if(kbd_length == 0) {
            memory_free(kbd_data);

            continue;
        }

        char_t data[4096] = {0};
        uint32_t data_idx = 0;

        kbd_ev_cnt = kbd_length / sizeof(kbd_report_t);

        for(uint32_t i = 0; i < kbd_ev_cnt; i++) {
            if(kbd_data[i].is_pressed) {
                if(kbd_data[i].is_printable) {
                    data_idx = windowmanager_append_wchar_to_buffer(kbd_data[i].key, data, data_idx);
                } else {
                    if(kbd_data[i].key == KBD_SCANCODE_BACKSPACE) {
                        data[data_idx++] = '\b';
                        data[data_idx++] = ' ';
                        data[data_idx++] = '\b';
                    }
                }
            }
        }

        data[data_idx] = NULL;

        memory_free(kbd_data);

        char_t last_char = data[4095];

        if(last_char != NULL) {
            data[4095] = NULL;
        }

        video_text_print(data);

        if(last_char != NULL) {
            char_t buffer[2] = {last_char, NULL};
            video_text_print(buffer);
        }

        data[4095] = last_char;
    }

    return 0;
}

uint64_t windowmanager_task_id = 0;

int8_t windowmanager_init(void) {
    memory_heap_t* heap = memory_get_default_heap();

    windowmanager_task_id = task_create_task(heap, 32 << 20, 64 << 10, windowmanager_main, 0, NULL, "windowmanager");

    while(!windowmanager_initialized) {
        cpu_sti();
        cpu_idle();
    }

    return windowmanager_task_id == -1ULL ? -1 : 0;
}
