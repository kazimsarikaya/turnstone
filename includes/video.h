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
#include <memory.h>
#include <linkedlist.h>

/*! magic for psf2 fonts*/
#define VIDEO_PSF2_FONT_MAGIC 0x864ab572
/*! magic for psf1 fonts*/
#define VIDEO_PSF1_FONT_MAGIC 0x0436

#define VIDEO_PCI_DEVICE_VENDOR_VIRTIO 0x1AF4
#define VIDEO_PCI_DEVICE_ID_VIRTIO_GPU 0x1050

#define VIDEO_PCI_DEVICE_VENDOR_VMWARE 0x15AD
#define VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2 0x0405

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
} __attribute__((packed)) video_psf2_font_t; ///< short hand for struct @ref video_psf2_font_s

/**
 * @struct video_psf1_font_t
 * @brief psf v1 font header
 */
typedef struct video_psf1_font_t {
    uint16_t magic; ///< magic bytes to identify PSF
    uint8_t  mode; ///< mode
    uint8_t  bytes_per_glyph; ///< size of each glyph
} __attribute__((packed)) video_psf1_font_t; ///< short hand for struct @ref video_psf1_font_s

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
void video_init(void);

/**
 * @brief updates frame buffer address for page mapping changes
 */
void video_refresh_frame_buffer_address(void);

/**
 * @brief clears screen aka write space to all buffer
 */
void video_clear_screen(void);

/**
 * @brief video diplay pci devices init
 * @param[in] heap heap to allocate memory
 * @param[in] display_controllers list of display controllers
 * @return 0 if success
 */
int8_t video_display_init(memory_heap_t* heap, linkedlist_t display_controllers);
int8_t video_copy_contents_to_frame_buffer(uint8_t* buffer, uint64_t new_width, uint64_t new_height, uint64_t new_pixels_per_scanline);

typedef void (*video_display_flush_f)(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

extern video_display_flush_f VIDEO_DISPLAY_FLUSH;

void video_set_color(uint32_t foreground, uint32_t background);

void video_print(const char_t* string);
#endif
