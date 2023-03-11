/**
 * @file video.h
 * @brief video out interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___VIDEO_H
/*! prevent duplicate header error macro */
#define ___VIDEO_H 0

#include <types.h>
#include <logging.h>

/*! magic for psf2 fonts*/
#define VIDEO_PSF2_FONT_MAGIC 0x864ab572
/*! magic for psf1 fonts*/
#define VIDEO_PSF1_FONT_MAGIC 0x0436

/**
 * @struct video_psf2_font_t
 * @brief psf v2 font header
 */
typedef struct video_psf2_font_t {
    uint32_t magic; ///< magic bytes to identify PSF
    uint32_t version; ///< zero
    uint32_t header_size; ///< offset of bitmaps in file, 32
    uint32_t flags; ///< 0 if there's no unicode table
    int32_t  glyph_count; ///< number of glyphs
    int32_t  bytes_per_glyph; ///< size of each glyph
    int32_t  height; ///< height in pixels
    int32_t  width; ///< width in pixels
} video_psf2_font_t; ///< short hand for struct @ref video_psf2_font_s

/**
 * @struct video_psf1_font_t
 * @brief psf v1 font header
 */
typedef struct video_psf1_font_t {
    uint16_t magic; ///< magic bytes to identify PSF
    uint8_t  mode; ///< mode
    uint8_t  bytes_per_glyph; ///< size of each glyph
}video_psf1_font_t; ///< short hand for struct @ref video_psf1_font_s

/**
 * @struct video_frame_buffer_t
 * @brief video frame buffer information
 */
typedef struct video_frame_buffer_t {
    uint64_t physical_base_address; ///< frame buffers physical base address
    uint64_t virtual_base_address; ///< frame buffers virtual base address
    uint64_t buffer_size; ///< frame buffer size
    uint32_t width; ///< width pixel size
    uint32_t height; ///< height pixel size
    uint32_t pixels_per_scanline; ///< how many pixels per line, it can be bigger than width, if not max resolution is set
} video_frame_buffer_t; ///< short hand for struct @ref video_frame_buffer_s

/*! pixel type */
typedef uint32_t pixel_t;

/**
 * @brief initialize video support on kernel
 */
void video_init();

/**
 * @brief updates frame buffer address for page mapping changes
 */
void video_refresh_frame_buffer_address();

/**
 * @brief writes string to video buffer
 * @param[in] string string will be writen
 *
 * string will be writen at current cursor position and cursor will be updated.
 */
void video_print(char_t* string);

/**
 * @brief clears screen aka write space to all buffer
 */
void video_clear_screen();

/**
 * @brief writes string to video buffer
 * @param[in] fmt string will be writen. this string can be a format
 * @param[in] ... variable length args
 * @return writen data chars
 *
 * format will be writen at current cursor position and cursor will be updated. also
 * variable args will be converted and written with help of format
 */
size_t video_printf(char_t* fmt, ...) __attribute__ ((format (printf, 1, 2)));

/*! printf macro that calls video_printf */
#define printf(...) video_printf(__VA_ARGS__)

/**
 * @brief kernel logging macro
 * @param[in] M module name @sa logging_modules_e
 * @param[in] L log level @sa logging_level_e
 * @param[in] msg log message, also if it contains after this there should be a variable arg list
 * @param[in] ... arguments for format in msg
 */
#define PRINTLOG(M, L, msg, ...)  if(LOG_NEED_LOG(M, L)) { \
            if(LOG_LOCATION) { video_printf("%s:%i:%s:%s: " msg "\n", __FILE__, __LINE__, logging_module_names[M], logging_level_names[L], ## __VA_ARGS__); } \
            else {video_printf("%s:%s: " msg "\n", logging_module_names[M], logging_level_names[L], ## __VA_ARGS__); } }


#endif
