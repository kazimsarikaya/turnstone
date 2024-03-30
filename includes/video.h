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
#include <list.h>
#include <graphics/image.h>
#include <graphics/font.h>

#define VIDEO_PCI_DEVICE_VENDOR_VIRTIO 0x1AF4
#define VIDEO_PCI_DEVICE_ID_VIRTIO_GPU 0x1050

#define VIDEO_PCI_DEVICE_VENDOR_VMWARE 0x15AD
#define VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2 0x0405

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
int8_t video_display_init(memory_heap_t* heap, list_t* display_controllers);
int8_t video_copy_contents_to_frame_buffer(uint8_t* buffer, uint64_t new_width, uint64_t new_height, uint64_t new_pixels_per_scanline);

typedef void (*video_display_flush_f)(uint32_t scanout, uint64_t offset, uint32_t x, uint32_t y, uint32_t width, uint32_t height);

extern video_display_flush_f VIDEO_DISPLAY_FLUSH;

typedef void (*video_move_cursor_f)(uint32_t x, uint32_t y);

extern video_move_cursor_f VIDEO_MOVE_CURSOR;

void video_set_color(color_t foreground, color_t background);

void video_print(const char_t* string);

int8_t video_move_text_cursor(int32_t x, int32_t y);
int8_t video_move_text_cursor_relative(int32_t x, int32_t y);
void   video_text_cursor_get(int32_t* x, int32_t* y);
void   video_text_cursor_toggle(boolean_t flush);
void   video_text_cursor_hide(void);
void   video_text_cursor_show(void);

graphics_raw_image_t* video_get_mouse_image(void);
#endif
