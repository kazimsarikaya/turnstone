/**
 * @file video.h
 * @brief video out interface
 */
#ifndef ___VIDEO_H
/*! prevent duplicate header error macro */
#define ___VIDEO_H 0

#include <types.h>
#include <logging.h>

#define VIDEO_PSF2_FONT_MAGIC 0x864ab572
#define VIDEO_PSF1_FONT_MAGIC 0x0436

typedef struct {
	uint32_t magic;           /* magic bytes to identify PSF */
	uint32_t version;         /* zero */
	uint32_t header_size;      /* offset of bitmaps in file, 32 */
	uint32_t flags;           /* 0 if there's no unicode table */
	int32_t glyph_count;        /* number of glyphs */
	int32_t bytes_per_glyph;   /* size of each glyph */
	int32_t height;          /* height in pixels */
	int32_t width;           /* width in pixels */
} video_psf2_font_t;

typedef struct {
	uint16_t magic;
	uint8_t mode;
	uint8_t bytes_per_glyph;
}video_psf1_font_t;

typedef struct {
	uint64_t physical_base_address;
	uint64_t virtual_base_address;
	uint64_t buffer_size;
	uint32_t width;
	uint32_t height;
	uint32_t pixels_per_scanline;
} video_frame_buffer_t;

typedef uint32_t pixel_t;

void video_init();
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
 * @param[in] args variable length args
 * @return writen data chars
 *
 * format will be writen at current cursor position and cursor will be updated. also
 * variable args will be converted and written with help of format
 */
size_t video_printf(char_t* fmt, ...);

#define printf(...) video_printf(__VA_ARGS__)

#define PRINTLOG(M, L, msg, ...)  if(LOG_NEED_LOG(M, L)) { \
		if(LOG_LOCATION) { video_printf("%s:%i:%s:%s: " msg "\n", __FILE__, __LINE__, logging_module_names[M], logging_level_names[L], __VA_ARGS__); } \
		else {video_printf("%s:%s: " msg "\n", logging_module_names[M], logging_level_names[L], __VA_ARGS__); } }


#endif
