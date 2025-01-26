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

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_PCI_DEVICE_VENDOR_VIRTIO 0x1AF4
#define VIDEO_PCI_DEVICE_ID_VIRTIO_GPU 0x1050

#define VIDEO_PCI_DEVICE_VENDOR_VMWARE 0x15AD
#define VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2 0x0405

/**
 * @brief video diplay pci devices init
 * @param[in] heap heap to allocate memory
 * @param[in] display_controllers list of display controllers
 * @return 0 if success
 */
int8_t video_display_init(memory_heap_t* heap, list_t* display_controllers);

void video_print(const char_t* string);
void video_text_print(const char_t* string);

typedef void (*video_graphics_print_f)(const char_t* str);

extern video_graphics_print_f VIDEO_GRAPHICS_PRINT;

#ifdef __cplusplus
}
#endif

#endif
