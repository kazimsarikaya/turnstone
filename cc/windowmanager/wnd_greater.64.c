/**
 * @file wnd_greater.64.c
 * @brief Window manager greater window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_greater.h>
#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <strings.h>
#include <graphics/screen.h>

MODULE("turnstone.windowmanager");

extern char_t tos_logo_data_start;

static int8_t wndmgr_rainbow_on_redraw(const window_event_t* event) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    window_t* window = event->window;

    if(window == NULL) {
        return -1;
    }

    uintptr_t angle_data_raw = (uintptr_t)window->extra_data;
    float32_t angle = (float32_t)angle_data_raw / 2;

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    sgfx_create_sub_context(gfx_ctx,
                            window->rect.x,
                            window->rect.y,
                            window->rect.width,
                            window->rect.height);


    sgfx_clear(gfx_ctx, 0.10f, 0.10f, 0.10f, 1.0f);

    sgfx_matrix_mode(gfx_ctx, SGFX_PROJECTION);
    sgfx_load_identity(gfx_ctx);
    sgfx_ortho_f32(gfx_ctx,
                   -1.0f, 1.0f,
                   -1.0f, 1.0f,
                   -1.0f, 1.0f);

    sgfx_matrix_mode(gfx_ctx, SGFX_MODELVIEW);
    sgfx_load_identity(gfx_ctx);
    sgfx_rotate_f32(gfx_ctx, angle, 0.0f, 0.0f, 1.0f);
    // sgfx_scale_f32(gfx_ctx, 0.5f, 0.5f, 1.0f);

    sgfx_begin(gfx_ctx, SGFX_TRIANGLES);
    sgfx_color4_f32(gfx_ctx, 1.0f, 0.0f, 0.0f, 1.0f);
    sgfx_vertex3_f32(gfx_ctx, 0.0f, 0.5f, 0.0f);
    sgfx_color4_f32(gfx_ctx, 0.0f, 1.0f, 0.0f, 1.0f);
    sgfx_vertex3_f32(gfx_ctx, -0.5f, -0.5f, 0.0f);
    sgfx_color4_f32(gfx_ctx, 0.0f, 0.0f, 1.0f, 1.0f);
    sgfx_vertex3_f32(gfx_ctx, 0.5f, -0.5f, 0.0f);
    sgfx_end(gfx_ctx);

    sgfx_destroy_sub_context(gfx_ctx);

    angle += 0.5f;

    if(angle >= 360.0f) {
        angle = 0.0f;
    }

    window->extra_data = (void*)(uintptr_t)(angle * 2);

    return 0;
}

window_t* windowmanager_create_greater_window(void) {
    windowmanager_t* wndmgr = windowmanager_get_instance();

    uint32_t font_width = wndmgr->font_width;
    uint32_t font_height = wndmgr->font_height;
    uint32_t screen_width = wndmgr->screen_width;
    uint32_t screen_height = wndmgr->screen_height;

    window_t* window = windowmanager_create_top_window();

    if(window == NULL) {
        return NULL;
    }

    char_t* windowmanager_turnstone_ascii_art = strdup((char_t*)&tos_logo_data_start);

    rect_t rect = windowmanager_calc_text_rect(windowmanager_turnstone_ascii_art, screen_width);
    rect.x = (screen_width - rect.width) / 2;
    rect.y = (screen_height - rect.height) / 2;
    // align x to font width, y to font height
    rect.x = (rect.x / font_width) * font_width;
    rect.y = (rect.y / font_height) * font_height;

    window_t* child = windowmanager_create_window(window,
                                                  windowmanager_turnstone_ascii_art,
                                                  rect,
                                                  (color_t){.color = 0x00000000},
                                                  (color_t){.color = 0xFF2288FF});

    if(child == NULL) {
        memory_free(window);
        return NULL;
    }

    int32_t old_x = rect.x;
    int32_t old_y = rect.y;
    int32_t old_height = rect.height;
    int32_t old_width = rect.width;

    char_t* text = strdup("Press F2 to open panel");

    rect = windowmanager_calc_text_rect(text, screen_width);
    rect.x = old_x;
    rect.y = old_y + old_height + 4 * font_height;

    child = windowmanager_create_window(window,
                                        text,
                                        rect,
                                        (color_t){.color = 0x00000000},
                                        (color_t){.color = 0xFF00FF00});

    if(child == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }


    rect_t rainbow_rect = {
        .x = old_x + old_width + font_width,
        .y = font_height,
        .width = screen_width - (old_x + old_width) - 3 * font_width,
        .height = old_y - 2 * font_height
    };

    window_t* rainbow_window = windowmanager_create_window(window,
                                                           NULL,
                                                           rainbow_rect,
                                                           (color_t){.color = 0xFFFFFFFF},
                                                           (color_t){.color = 0xFFFFFFFF});

    if(rainbow_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    rainbow_window->extra_data = (void*)(uintptr_t)0;

    rainbow_window->on_redraw = wndmgr_rainbow_on_redraw;
    rainbow_window->is_always_redrawn = true;


    rainbow_rect = (rect_t){
        .x = screen_width - 300 - font_width,
        .y = screen_height - 300 - font_height,
        .width = 300,
        .height = 300
    };

    rainbow_window = windowmanager_create_window(window,
                                                 NULL,
                                                 rainbow_rect,
                                                 (color_t){.color = 0xFFFFFFFF},
                                                 (color_t){.color = 0xFFFFFFFF});

    if(rainbow_window == NULL) {
        windowmanager_destroy_window(window);
        return NULL;
    }

    rainbow_window->extra_data = (void*)(uintptr_t)0;

    rainbow_window->on_redraw = wndmgr_rainbow_on_redraw;
    rainbow_window->is_always_redrawn = true;

    return window;
}
