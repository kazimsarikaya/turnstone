/**
 * @file video_fb.h
 * @brief video frame buffer driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___VIDEO_FB_H
/*! prevent duplicate header error macro */
#define ___VIDEO_FB_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief initialize video support on kernel
 */
void video_fb_init(void);

/**
 * @brief updates frame buffer address for page mapping changes
 */
void video_fb_refresh_frame_buffer_address(void);

int8_t video_fb_copy_contents_to_frame_buffer(uint8_t* buffer, uint64_t new_width, uint64_t new_height, uint64_t new_pixels_per_scanline);

#ifdef __cplusplus
}
#endif

#endif
