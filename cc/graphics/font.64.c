/**
 * @file font.64.c
 * @brief Font library implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <graphics/font.h>
#include <utils.h>
#include <memory.h>

MODULE("turnstone.kernel.graphics.font");


extern font_psf2_t font_psf2_data_start;
extern font_psf2_t font_psf2_data_end;

static uint8_t* FONT_ADDRESS = NULL;
static int32_t FONT_WIDTH = 0;
static int32_t FONT_HEIGHT = 0;
static int32_t FONT_BYTES_PER_GLYPH = 0;
static uint32_t FONT_MASK = 0;
static uint32_t FONT_BYTES_PERLINE = 0;

static const uint32_t FONT_TABLE_COLUMNS = 32;
static uint32_t FONT_TABLE_GLYPH_COUNT = 0;
static font_table_t* font_table = NULL;

static wchar_t* font_unicode_table = NULL;

static wchar_t font_lookup_unicode(wchar_t wc) {
    if(wc == 0) {
        return 0;
    }

    if(font_unicode_table == NULL) {
        return wc;
    }

    if(font_unicode_table[wc] == 0) {
        return wc;
    }

    return font_unicode_table[wc];
}

static void font_print_glyph_with_stride_raw(wchar_t wc,
                                             color_t foreground, color_t background,
                                             uint8_t* font_address,
                                             pixel_t* base_address,
                                             uint32_t x, uint32_t y,
                                             uint32_t stride,
                                             uint32_t font_width, uint32_t font_height,
                                             uint32_t font_bytes_per_glyph,
                                             uint32_t font_bytes_perline,
                                             uint32_t font_mask) {
    // wc = font_lookup_unicode(wc);

    uint8_t* glyph = font_address + (wc * font_bytes_per_glyph);

    int32_t offs = (y * font_height * stride) + (x * font_width);

    uint32_t lx, ly, line, mask;

    for(ly = 0; ly < font_height; ly++) {
        line = offs;
        mask = font_mask;

        uint32_t tmp = byte_swap(*((uint64_t*)glyph), font_bytes_perline);

        for(lx = 0; lx < font_width; lx++) {

            *(base_address + line) = (pixel_t)(tmp & mask ? foreground.color : background.color);

            mask >>= 1;
            line++;
        }

        glyph += font_bytes_perline;
        offs  += stride;
    }
}

void font_print_glyph_with_stride(wchar_t wc,
                                  color_t foreground, color_t background,
                                  pixel_t* destination_base_address,
                                  uint32_t x, uint32_t y,
                                  uint32_t stride) {
    font_print_glyph_with_stride_raw(wc,
                                     foreground, background,
                                     FONT_ADDRESS,
                                     destination_base_address,
                                     x, y,
                                     stride,
                                     FONT_WIDTH, FONT_HEIGHT,
                                     FONT_BYTES_PER_GLYPH,
                                     FONT_BYTES_PERLINE,
                                     FONT_MASK);
}

static void video_build_font_table(font_psf2_t* font) {
    uint32_t col_count = FONT_TABLE_COLUMNS;
    FONT_TABLE_GLYPH_COUNT = font->glyph_count;
    uint32_t row_count = font->glyph_count / col_count;

    if(font->glyph_count % col_count) {
        row_count++;
    }

    uint32_t stride = col_count * FONT_WIDTH;

    font_table = memory_malloc(sizeof(font_table_t));

    if(!font_table) {
        return;
    }

    font_table->bitmap = memory_malloc(sizeof(pixel_t) * col_count * FONT_WIDTH * row_count * FONT_HEIGHT);

    if(!font_table->bitmap) {
        memory_free(font_table);
        return;
    }

    font_table->font_width = FONT_WIDTH;
    font_table->font_height = FONT_HEIGHT;
    font_table->column_count = col_count;
    font_table->row_count = row_count;
    font_table->glyph_count = font->glyph_count;

    color_t foreground = { .color = 0xFFFFFFFF };
    color_t background = { .color = 0x00000000 };

    if(font_table) {
        for(int32_t i = 0; i < font->glyph_count; i++) {
            uint32_t x = i % col_count;
            uint32_t y = i / col_count;

            font_print_glyph_with_stride(i, foreground, background, font_table->bitmap, x, y, stride);
        }
    }
}


font_table_t* font_get_font_table(void) {
    return font_table;
}

int8_t font_init(void) {
    font_psf2_t* font2 = &font_psf2_data_start;

    if(font2) {
        if(font2->magic == FONT_PSF2_MAGIC) {
            FONT_ADDRESS = ((uint8_t*)&font_psf2_data_start) + font2->header_size;
            FONT_WIDTH = font2->width;
            FONT_HEIGHT = font2->height;
            FONT_BYTES_PER_GLYPH = font2->bytes_per_glyph;

            if(font2->flags) {
                wchar_t glyph = 0;
                font_unicode_table = memory_malloc(sizeof(wchar_t) * ((wchar_t)-1));
                uint8_t* font_loc = FONT_ADDRESS + font2->glyph_count * font2->bytes_per_glyph;

                while(font_loc < (uint8_t*)&font_psf2_data_end) {
                    wchar_t wc = *font_loc;

                    if(wc == 0xFF) {
                        glyph++;
                        font_loc++;

                        continue;
                    } else if(wc & 128) {
                        if((wc & 32) == 0 ) {
                            wc = ((font_loc[0] & 0x1F) << 6) + (font_loc[1] & 0x3F);
                            font_loc++;
                        } else if((wc & 16) == 0 ) {
                            wc = ((((font_loc[0] & 0xF) << 6) + (font_loc[1] & 0x3F)) << 6) + (font_loc[2] & 0x3F);
                            font_loc += 2;
                        } else if((wc & 8) == 0 ) {
                            wc = ((((((font_loc[0] & 0x7) << 6) + (font_loc[1] & 0x3F)) << 6) + (font_loc[2] & 0x3F)) << 6) + (font_loc[3] & 0x3F);
                            font_loc += 3;
                        } else {
                            wc = 0;
                        }

                    }

                    font_unicode_table[wc] = glyph;
                    font_loc++;
                }

            }

            FONT_BYTES_PERLINE = (FONT_WIDTH + 7) / 8;

            FONT_MASK = 1 << (FONT_BYTES_PERLINE * 8 - 1);

            video_build_font_table(font2);

        } else {

            video_psf1_font_t* font1 = (video_psf1_font_t*)&font_psf2_data_start;

            if(font1->magic == FONT_PSF1_MAGIC) {
                uint64_t addr = (uint64_t)&font_psf2_data_start;
                addr += sizeof(video_psf1_font_t);
                FONT_ADDRESS = (uint8_t*)addr;
                FONT_WIDTH = 10;
                FONT_HEIGHT = 20;
                FONT_BYTES_PER_GLYPH = font1->bytes_per_glyph;

                if(font1->mode) {
                    wchar_t glyph = 0;
                    font_unicode_table = memory_malloc(sizeof(wchar_t) * ((wchar_t)-1));
                    uint8_t* font_loc = FONT_ADDRESS + 512 * font1->bytes_per_glyph;

                    while(font_loc < (uint8_t*)&font_psf2_data_end) {
                        wchar_t wc = *font_loc;

                        if(wc == 0xFF) {
                            glyph++;
                            font_loc++;

                            continue;
                        } else if(wc & 128) {
                            if((wc & 32) == 0 ) {
                                wc = ((font_loc[0] & 0x1F) << 6) + (font_loc[1] & 0x3F);
                                font_loc++;
                            } else if((wc & 16) == 0 ) {
                                wc = ((((font_loc[0] & 0xF) << 6) + (font_loc[1] & 0x3F)) << 6) + (font_loc[2] & 0x3F);
                                font_loc += 2;
                            } else if((wc & 8) == 0 ) {
                                wc = ((((((font_loc[0] & 0x7) << 6) + (font_loc[1] & 0x3F)) << 6) + (font_loc[2] & 0x3F)) << 6) + (font_loc[3] & 0x3F);
                                font_loc += 3;
                            } else {
                                wc = 0;
                            }

                        }

                        font_unicode_table[wc] = glyph;
                        font_loc++;
                    }

                }

                FONT_BYTES_PERLINE = (FONT_WIDTH + 7) / 8;

                FONT_MASK = 1 << (FONT_BYTES_PERLINE * 8 - 1);

            } else {
                return -2;
            }

        }
    } else {
        return -1;
    }

    return 0;
}

wchar_t font_get_wc(const char_t* string, int64_t * idx) {
    if(string == NULL || idx == NULL) {
        return NULL;
    }

    wchar_t wc = string[0];

    if(wc & 128) {
        if((wc & 32) == 0) {
            wc = ((string[0] & 0x1F) << 6) + (string[1] & 0x3F);
            (*idx)++;
        } else if((wc & 16) == 0 ) {
            wc = ((((string[0] & 0xF) << 6) + (string[1] & 0x3F)) << 6) + (string[2] & 0x3F);
            (*idx) += 2;
        } else if((wc & 8) == 0 ) {
            wc = ((((((string[0] & 0x7) << 6) + (string[1] & 0x3F)) << 6) + (string[2] & 0x3F)) << 6) + (string[3] & 0x3F);
            (*idx) += 3;
        } else {
            wc = 0;
        }
    }

    return font_lookup_unicode(wc);
}

void font_get_font_dimension(uint32_t* width, uint32_t* height) {
    if(width) {
        *width = FONT_WIDTH;
    }

    if(height) {
        *height = FONT_HEIGHT;
    }
}
