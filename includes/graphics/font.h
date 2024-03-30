/**
 * @file font.h
 * @brief font interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___FONT_H
/*! prevent duplicate header error macro */
#define ___FONT_H 0

#include <types.h>

/*! magic for psf2 fonts*/
#define FONT_PSF2_MAGIC 0x864ab572
/*! magic for psf1 fonts*/
#define FONT_PSF1_MAGIC 0x0436

/*! pixel type */
typedef uint32_t pixel_t;

typedef union color_t color_t;

union color_t {
    struct {
        uint8_t alpha;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    } __attribute__((packed));
    uint32_t color;
};

/**
 * @struct font_psf2_t
 * @brief psf v2 font header
 */
typedef struct font_psf2_t {
    uint32_t magic; ///< magic bytes to identify PSF
    uint32_t version; ///< zero
    uint32_t header_size; ///< offset of bitmaps in file, 32
    uint32_t flags; ///< 0 if there's no unicode table
    int32_t  glyph_count; ///< number of glyphs
    int32_t  bytes_per_glyph; ///< size of each glyph
    int32_t  height; ///< height in pixels
    int32_t  width; ///< width in pixels
} __attribute__((packed)) font_psf2_t; ///< short hand for struct @ref font_psf2_t

/**
 * @struct font_psf1_t
 * @brief psf v1 font header
 */
typedef struct font_psf1_t {
    uint16_t magic; ///< magic bytes to identify PSF
    uint8_t  mode; ///< mode
    uint8_t  bytes_per_glyph; ///< size of each glyph
} __attribute__((packed)) video_psf1_font_t; ///< short hand for struct @ref font_psf1_t

/**
 * @struct font_table_t
 * @brief font table
 */
typedef struct font_table_t {
    pixel_t * bitmap; ///< bitmap of font
    uint32_t  font_width; ///< width of font
    uint32_t  font_height; ///< height of font
    uint32_t  glyph_count; ///< number of glyphs
    uint32_t  column_count; ///< number of columns
    uint32_t  row_count; ///< number of rows
} font_table_t; ///< short hand for struct @ref font_table_s

void font_print_glyph_with_stride(wchar_t wc,
                                  color_t foreground, color_t background,
                                  pixel_t* destination_base_address,
                                  uint32_t x, uint32_t y,
                                  uint32_t stride);

font_table_t* font_get_font_table(void);

wchar_t font_get_wc(const char_t* string, int64_t* idx);

int8_t font_init(void);

void font_get_font_dimension(uint32_t* width, uint32_t* height);

#endif /* font.h */
