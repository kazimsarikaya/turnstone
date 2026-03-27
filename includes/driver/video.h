/**
 * @file video.h
 * @brief video driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___VIDEO_H
#define ___VIDEO_H 0

#include <types.h>
#include <memory.h>
#include <list.h>
#include <graphics/color.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_PCI_DEVICE_VENDOR_VIRTIO 0x1AF4
#define VIDEO_PCI_DEVICE_ID_VIRTIO_GPU 0x1050

#define VIDEO_PCI_DEVICE_VENDOR_VMWARE 0x15AD
#define VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2 0x0405

#define VIDEO_PCI_DEVICE_VENDOR_QEMU 0x1234
#define VIDEO_PCI_DEVICE_ID_QEMU_VGA 0x1111


/**
 * @brief video diplay pci devices init
 * @param[in] heap heap to allocate memory
 * @param[in] display_controllers list of display controllers
 * @return 0 if success
 */
int8_t video_display_init(memory_heap_t* heap, list_t* display_controllers);
int8_t video_display_reinit(void);

void video_print(const char_t* string);
void video_text_print(const char_t* string);

void      video_set_graphics_mode(boolean_t enabled);
boolean_t video_is_graphics_mode(void);
int8_t    video_configure_lock(void);
void      video_acquire_lock(void);
void      video_release_lock(void);
color_t*  video_get_frame_buffer_base_address(void);

typedef void (*video_graphics_print_f)(const char_t* str);

extern video_graphics_print_f VIDEO_GRAPHICS_PRINT;

#ifdef __cplusplus
}
#endif

#endif
