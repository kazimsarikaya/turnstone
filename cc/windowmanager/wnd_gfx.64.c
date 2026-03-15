/**
 * @file wnd_gfx.64.c
 * @brief Window manager graphics implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <windowmanager.h>
#include <windowmanager/wnd_gfx.h>
#include <windowmanager/wnd_utils.h>
#include <graphics/screen.h>
#include <graphics/font.h>
#include <graphics/font_atlas.h>
#include <graphics/text_cursor.h>
#include <device/mouse.h>
#include <logging.h>
#include <strings.h>
#include <math.h>

MODULE("turnstone.windowmanager");

void video_text_print(const char_t* text);

extern color_t* VIDEO_BASE_ADDRESS;


int8_t wndmgr_mouse_init(windowmanager_t* wndmgr) {
    graphics_raw_image_t* wndmgr_mouse_image = mouse_get_image();

    if(wndmgr_mouse_image == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to get mouse image\n");
        return -1;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image loaded with size %dx%d", wndmgr_mouse_image->width, wndmgr_mouse_image->height);
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image first pixel: 0x%x", wndmgr_mouse_image->data[0].color);
    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Mouse image last pixel: 0x%x", wndmgr_mouse_image->data[wndmgr_mouse_image->width * wndmgr_mouse_image->height - 1].color);

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    wndmgr->mouse_texture = sgfx_gen_texture(gfx_ctx);
    sgfx_bind_texture(gfx_ctx, wndmgr->mouse_texture);
    sgfx_tex_image2d(gfx_ctx, wndmgr_mouse_image->width, wndmgr_mouse_image->height, wndmgr_mouse_image->data);
    sgfx_bind_texture(gfx_ctx, 0);

    wndmgr->mouse_image_width  = wndmgr_mouse_image->width;
    wndmgr->mouse_image_height = wndmgr_mouse_image->height;

    wndmgr->mouse_x = (wndmgr->screen_width - wndmgr->mouse_image_width) / 2;
    wndmgr->mouse_y = (wndmgr->screen_height - wndmgr->mouse_image_height) / 2;

    wndmgr->mouse_initialized = true;

    return 0;
}

int8_t wndmgr_font_init(windowmanager_t* wndmgr) {
    boolean_t use_old_font = false;

    font_table_t* old_font = font_get_font_table();

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Old font has size %dx%d, columns: %d, rows: %d, glyphs: %d",
             old_font->font_width,
             old_font->font_height,
             old_font->column_count,
             old_font->row_count,
             old_font->glyph_count);

    font_table_t* new_font = font_atlas_get_font_table();

    if(new_font == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to get font table\n");
        return -1;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "New font has size %dx%d, columns: %d, rows: %d, glyphs: %d",
             new_font->font_width,
             new_font->font_height,
             new_font->column_count,
             new_font->row_count,
             new_font->glyph_count);

    font_table_t* font = use_old_font ? old_font : new_font;

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Font loaded with size %dx%d, columns: %d, rows: %d, glyphs: %d",
             font->font_width,
             font->font_height,
             font->column_count,
             font->row_count,
             font->glyph_count);

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    wndmgr->font_texture = sgfx_gen_texture(gfx_ctx);
    sgfx_bind_texture(gfx_ctx, wndmgr->font_texture);

    if(use_old_font) {
        sgfx_tex_image2d(gfx_ctx, font->font_width * font->column_count, font->font_height * font->row_count, font->color_data);
    } else {
        sgfx_tex_sdf(gfx_ctx, font->font_width * font->column_count, font->font_height * font->row_count, font->float_data);
    }

    sgfx_bind_texture(gfx_ctx, 0);

    wndmgr->font_is_sdf = !use_old_font;

    wndmgr->font_column_count = font->column_count;
    wndmgr->font_row_count    = font->row_count;
    wndmgr->font_real_width   = font->font_width;
    wndmgr->font_real_height  = font->font_height;

    wndmgr->font_width  = old_font->font_width;
    wndmgr->font_height = old_font->font_height;

    uint32_t sheet_tile_size = math_lcm(wndmgr->font_width, wndmgr->font_height);
    wndmgr->sheet_tile_size = sheet_tile_size;

    while(wndmgr->sheet_tile_size < 64) {
        wndmgr->sheet_tile_size += sheet_tile_size;
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Font width and height set to %dx%d", wndmgr->font_width, wndmgr->font_height);

    wndmgr->font_uv_table = memory_malloc(sizeof(wndmgr_font_uv_t) * font->glyph_count);

    if(wndmgr->font_uv_table == NULL) {
        PRINTLOG(WINDOWMANAGER, LOG_ERROR, "Failed to allocate memory for font UV table\n");
        return -2;
    }

    if(use_old_font) {
        // manually create uv table for old font
        for(uint32_t i = 0; i < font->glyph_count; i++) {
            uint32_t col = i % font->column_count;
            uint32_t row = i / font->column_count;

            wndmgr->font_uv_table[i].u0 = (float32_t)(col * font->font_width) / (float32_t)(font->font_width * font->column_count);
            wndmgr->font_uv_table[i].v0 = (float32_t)(row * font->font_height) / (float32_t)(font->font_height * font->row_count);
            wndmgr->font_uv_table[i].u1 = (float32_t)((col + 1) * font->font_width) / (float32_t)(font->font_width * font->column_count);
            wndmgr->font_uv_table[i].v1 = (float32_t)((row + 1) * font->font_height) / (float32_t)(font->font_height * font->row_count);
        }
    } else {
        for(uint32_t i = 0; i < font->glyph_count; i++) {
            wndmgr->font_uv_table[i].u0 = font_glyphs[i].u0;
            wndmgr->font_uv_table[i].v0 = font_glyphs[i].v0;
            wndmgr->font_uv_table[i].u1 = font_glyphs[i].u1;
            wndmgr->font_uv_table[i].v1 = font_glyphs[i].v1;
        }
    }

    PRINTLOG(WINDOWMANAGER, LOG_INFO, "Font texture created with size %dx%d", font->font_width * font->column_count, font->font_height * font->row_count);

    return 0;

}

static void wndmgr_mouse_draw_cursor(windowmanager_t* wndmgr) {
    if (!wndmgr->mouse_initialized) {
        return;
    }

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    float32_t w = (float32_t)wndmgr->mouse_image_width;
    float32_t h = (float32_t)wndmgr->mouse_image_height;

    sgfx_bind_texture(gfx_ctx, wndmgr->mouse_texture);
    sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_QUADS);
    sgfx_color4_f32(gfx_ctx, 1.0f, 1.0f, 1.0f, 1.0f);

    sgfx_texcoord2_f32(gfx_ctx, 0.0f, 0.0f);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)wndmgr->mouse_x, (float32_t)wndmgr->mouse_y, 0.0f);

    sgfx_texcoord2_f32(gfx_ctx, 1.0f, 0.0f);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)(wndmgr->mouse_x + w), (float32_t)wndmgr->mouse_y, 0.0f);

    sgfx_texcoord2_f32(gfx_ctx, 1.0f, 1.0f);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)(wndmgr->mouse_x + w), (float32_t)(wndmgr->mouse_y + h), 0.0f);

    sgfx_texcoord2_f32(gfx_ctx, 0.0f, 1.0f);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)wndmgr->mouse_x, (float32_t)(wndmgr->mouse_y + h), 0.0f);

    sgfx_end(gfx_ctx);
    sgfx_bind_texture(gfx_ctx, 0);
}

static void wndmgr_draw_text_cursor(windowmanager_t* wndmgr) {
    const window_sheet_t* tcs = NULL;
    if(wndmgr_find_window_sheet_by_text_cursor(wndmgr->current_window, &tcs)) {
        if(tcs == NULL) {
            return;
        }

        if(!tcs->is_drawing_occured) {
            return;
        }
    }

    int32_t cursor_x, cursor_y;
    text_cursor_get(&cursor_x, &cursor_y);

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    // Draw filled quad at cursor position
    sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_QUADS);
    sgfx_color4_f32(gfx_ctx, 1.0f, 1.0f, 1.0f, 0.5f); // White

    float32_t x0 = (float32_t)(cursor_x * wndmgr->font_width);
    float32_t y0 = (float32_t)(cursor_y * wndmgr->font_height);
    float32_t x1 = x0 + wndmgr->font_width;
    float32_t y1 = y0 + wndmgr->font_height;

    sgfx_vertex3_f32(gfx_ctx, x0, y0, 0.0f); // Top-left
    sgfx_vertex3_f32(gfx_ctx, x1, y0, 0.0f); // Top-right
    sgfx_vertex3_f32(gfx_ctx, x1, y1, 0.0f); // Bottom-right
    sgfx_vertex3_f32(gfx_ctx, x0, y1, 0.0f); // Bottom-left

    sgfx_end(gfx_ctx);
}

static void windowmanager_print_glyph(const windowmanager_t* wndmgr, uint32_t x, uint32_t y, char16_t wc) {
    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    float32_t u0 = wndmgr->font_uv_table[wc].u0;
    float32_t v0 = wndmgr->font_uv_table[wc].v0;
    float32_t u1 = wndmgr->font_uv_table[wc].u1;
    float32_t v1 = wndmgr->font_uv_table[wc].v1;

    sgfx_texcoord2_f32(gfx_ctx, u0, v0);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)(x * wndmgr->font_width),
                     (float32_t)(y * wndmgr->font_height), 0.0f);
    sgfx_texcoord2_f32(gfx_ctx, u1, v0);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)((x + 1) * wndmgr->font_width),
                     (float32_t)(y * wndmgr->font_height), 0.0f);
    sgfx_texcoord2_f32(gfx_ctx, u1, v1);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)((x + 1) * wndmgr->font_width),
                     (float32_t)((y + 1) * wndmgr->font_height), 0.0f);
    sgfx_texcoord2_f32(gfx_ctx, u0, v1);
    sgfx_vertex3_f32(gfx_ctx, (float32_t)(x * wndmgr->font_width),
                     (float32_t)((y + 1) * wndmgr->font_height), 0.0f);

}

static void windowmanager_print_text(const windowmanager_t* wndmgr, const window_sheet_t* sheet,
                                     uint32_t x, uint32_t y, const char16_t* text) {
    if(!sheet || !text) {
        return;
    }

    if(sheet->rect.x + (int64_t)x >= wndmgr->screen_width || sheet->rect.y + (int64_t)y >= wndmgr->screen_height) {
        return;
    }

    uint32_t font_width = wndmgr->font_width, font_height = wndmgr->font_height;

    uint32_t cur_x = 0;
    uint32_t cur_y = 0;

    uint32_t max_cur_x = sheet->rect.width / font_width;
    uint32_t max_cur_y = sheet->rect.height / font_height;

    if(cur_x >= max_cur_x || cur_y >= max_cur_y) {
        return;
    }

    color_t fg = sheet->foreground_color;

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    sgfx_bind_texture(gfx_ctx, wndmgr->font_texture);

    sgfx_color4_f32(gfx_ctx,
                    fg.red / 255.0f,
                    fg.green / 255.0f,
                    fg.blue / 255.0f,
                    fg.alpha / 255.0f);

    sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_QUADS);

    int64_t i = 0;

    while(text[i]) {
        char16_t wc;

        if(wndmgr->font_is_sdf) {
            wc = font_atlas_lookup_unicode(text[i]);
        } else {
            wc = font_lookup_unicode(text[i]);
        }

        if(wc == '\n') {
            cur_y += 1;

            if(cur_y >= max_cur_y) {
                break;
            }

            cur_x = 0;
        } else if(wc == '\r') {
            cur_x = 0;
        } else {
            windowmanager_print_glyph(wndmgr, cur_x, cur_y, wc);

            cur_x += 1;

            if(cur_x >= max_cur_x && text[i + 1] && text[i + 1] != '\n') {
                cur_y += 1;

                if(cur_y >= max_cur_y) {
                    break;
                }

                cur_x = 0;
            }
        }

        i++;
    }

    sgfx_end(gfx_ctx);

    sgfx_bind_texture(gfx_ctx, 0);
}

static void windowmanager_draw_window_internal(windowmanager_t* wndmgr, window_t* parent, window_t* window) {
    if(window == NULL) {
        return;
    }

    if(window->is_hidden) {
        return;
    }

    UNUSED(parent);

    if(window->on_predraw) {
        window_event_t event = {.type = WINDOW_EVENT_TYPE_PREDRAW, .window = window};
        window->on_predraw(&event);
    }

    sgfx_context_t* gfx_ctx = wndmgr->gfx_ctx;

    for(size_t i = 0; i < list_size(window->sheets); i++) {
        window_sheet_t* sheet = (window_sheet_t*)list_get_data_at_position(window->sheets, i);

        if(sheet->is_dirty || sheet->is_always_redrawn) {
            if(window->on_draw) {
                window_event_t event = {.type = WINDOW_EVENT_TYPE_DRAW, .window = window};
                window->on_draw(&event);
            } else {
                rect_t rect = sheet->absolute_rect;

                sgfx_create_sub_context(gfx_ctx, rect.x, rect.y, rect.width, rect.height);

                sgfx_clear_color(gfx_ctx, sheet->background_color);

                windowmanager_print_text(wndmgr, sheet, 0, 0, sheet->text);

                if(sheet->is_writable) {
                    sgfx_color4_f32(gfx_ctx, sheet->foreground_color.red / 255.0f,
                                    sheet->foreground_color.green / 255.0f,
                                    sheet->foreground_color.blue / 255.0f,
                                    sheet->foreground_color.alpha / 255.0f);
                    sgfx_begin(gfx_ctx, SGFX_DRAW_MODE_LINES);

                    // line at bottom of sheet
                    sgfx_vertex3_f32(gfx_ctx, 0.0f, (float32_t)sheet->rect.height - 1.0f, 0.0f);
                    sgfx_vertex3_f32(gfx_ctx, (float32_t)sheet->rect.width, (float32_t)sheet->rect.height - 1.0f, 0.0f);

                    sgfx_end(gfx_ctx);
                }


                sgfx_destroy_sub_context(gfx_ctx);
            }

            sheet->is_dirty           = false;
            sheet->is_drawing_occured = true;
        } else {
            sheet->is_drawing_occured = false;
        }

    }

    for (size_t i = 0; i < list_size(window->children); i++) {
        window_t* child = (window_t*)list_get_data_at_position(window->children, i);

        windowmanager_draw_window_internal(wndmgr, window, child);
    }

    return;
}

void windowmanager_draw_window(windowmanager_t* wndmgr, window_t* window) {
    if(wndmgr == NULL || window == NULL) {
        return;
    }

    windowmanager_draw_window_internal(wndmgr, NULL, window);
    wndmgr_draw_text_cursor(wndmgr);
    wndmgr_mouse_draw_cursor(wndmgr);
}
