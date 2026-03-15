/**
 * @file wnd_greater.64.c
 * @brief Window manager greater window implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager/wnd_create_destroy.h>
#include <windowmanager/wnd_utils.h>
#include <strings.h>
#include <graphics/screen.h>
#include <logging.h>

MODULE("turnstone.windowmanager");

extern char_t tos_logo_data_start;

static int8_t wndmgr_rainbow_on_draw(const window_event_t* event) {
    window_t* window = event->window;

    if(!window) {
        return -1;
    }

    windowmanager_t* wndmgr = window->wndmgr;

    uintptr_t angle_data_raw = (uintptr_t)window->extra_data;
    float32_t angle          = (float32_t)angle_data_raw / 2;

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    sgfx_create_sub_context(gfx_ctx,
                            window->owner_absolute_rect.x,
                            window->owner_absolute_rect.y,
                            window->owner_absolute_rect.width,
                            window->owner_absolute_rect.height);


    sgfx_clear(gfx_ctx, 0.10f, 0.10f, 0.10f, 1.0f);

    sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_PROJECTION);
    sgfx_load_identity(gfx_ctx);
    sgfx_ortho_f32(gfx_ctx,
                   -1.0f, 1.0f,
                   -1.0f, 1.0f,
                   -1.0f, 1.0f);

    sgfx_matrix_mode(gfx_ctx, SGFX_MATRIX_MODE_MODELVIEW);
    sgfx_load_identity(gfx_ctx);
    sgfx_rotate_f32(gfx_ctx, angle, 0.0f, 0.0f, 1.0f);
    // sgfx_scale_f32(gfx_ctx, 0.5f, 0.5f, 1.0f);

    sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_TRIANGLES);
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

    uint32_t font_width  = wndmgr->font_width;
    uint32_t font_height = wndmgr->font_height;

    window_top_window_t top_window = windowmanager_create_top_window(NULL, false);

    if(!top_window.main_window || !top_window.inside_window) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create greater window top window\n");
        return NULL;
    }

    window_t* window = top_window.inside_window;

    uint32_t window_width  = window->owner_rect.width;
    uint32_t window_height = window->owner_rect.height;

    char_t* windowmanager_turnstone_ascii_art = strdup((char_t*)&tos_logo_data_start);

    rect_t rect = wndmgr_calc_text_rect(windowmanager_turnstone_ascii_art, window_width);

    if(rect.width == 0 || rect.height == 0) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to calculate text rect for greater window\n");
        return NULL;
    }

    if(rect.width > window_width - 2 * font_width ||
       rect.height > window_height - 2 * font_height) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Greater window text too large for screen\n");
        return NULL;
    }

    rect.x = (window_width - rect.width) / 2;
    rect.y = (window_height - rect.height) / 2;
    // align x to font width, y to font height
    rect.x = (rect.x / font_width) * font_width;
    rect.y = (rect.y / font_height) * font_height;

    window_t* child = windowmanager_create_window(window,
                                                  rect,
                                                  (color_t){.color = 0xFF2288FF},
                                                  .text = windowmanager_turnstone_ascii_art);

    if(child == NULL) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create greater window child\n");
        return NULL;
    }

    int32_t old_x      = rect.x;
    int32_t old_y      = rect.y;
    int32_t old_height = rect.height;
    int32_t old_width  = rect.width;

    const char_t* text = "Press F2 to open panel";

    rect   = wndmgr_calc_text_rect(text, window_width);
    rect.x = old_x;
    rect.y = old_y + old_height + 4 * font_height;

    child = windowmanager_create_window(window,
                                        rect,
                                        (color_t){.color = 0xFF00FF00},
                                        .text = text);

    if(child == NULL) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create greater window instruction child\n");
        return NULL;
    }


    rect_t rainbow_rect = {
        .x      = old_x + old_width + font_width,
        .y      = font_height,
        .width  = window_width - (old_x + old_width) - 3 * font_width,
        .height = old_y - 2 * font_height
    };

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Rainbow rect: x=%d y=%d w=%d h=%d", rainbow_rect.x, rainbow_rect.y, rainbow_rect.width, rainbow_rect.height);

    window_t* rainbow_window = windowmanager_create_window(window,
                                                           rainbow_rect,
                                                           .is_single_sheet = true);

    if(rainbow_window == NULL) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create greater window rainbow child\n");
        return NULL;
    }

    rainbow_window->extra_data = (void*)(uintptr_t)0;

    rainbow_window->on_draw = wndmgr_rainbow_on_draw;
    wndmgr_mark_window_sheets_always_redrawn(rainbow_window, true);


    rainbow_rect = (rect_t){
        .x      = window_width - 300 - font_width,
        .y      = window_height - 300 - font_height,
        .width  = 300,
        .height = 300
    };

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Rainbow rect: x=%d y=%d w=%d h=%d", rainbow_rect.x, rainbow_rect.y, rainbow_rect.width, rainbow_rect.height);

    rainbow_window = windowmanager_create_window(window,
                                                 rainbow_rect,
                                                 .is_single_sheet = true);

    if(rainbow_window == NULL) {
        windowmanager_destroy_window(top_window.main_window);
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to create greater window rainbow child\n");
        return NULL;
    }

    rainbow_window->extra_data = (void*)(uintptr_t)0;

    rainbow_window->on_draw = wndmgr_rainbow_on_draw;
    wndmgr_mark_window_sheets_always_redrawn(rainbow_window, true);

    return top_window.main_window;
}
