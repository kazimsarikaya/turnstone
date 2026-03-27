/**
 * @file video_qemuvga.h
 * @brief qemu vga video driver header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __VIDEO_QEMUVGA_H
#define __VIDEO_QEMUVGA_H 0

#include <types.h>
#include <pci.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_QEMU_VGA_EDID_OFFSET  0x0000
#define VIDEO_QEMU_VGA_EDID_SIZE    0x0400

#define VIDEO_QEMU_VGA_IOPORT_OFFSET 0x0400
#define VIDEO_QEMU_VGA_IOPORT_SIZE   0x0020

#define VIDEO_QEMU_VGA_BOCHS_DISPI_OFFSET 0x0500
#define VIDEO_QEMU_VGA_BOCHS_DISPI_SIZE   0x0016

#define VIDEO_QEMU_VGA_QEXT_REGS_OFFSET 0x0600
#define VIDEO_QEMU_VGA_QEXT_REGS_SIZE   0x0008

#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_ID       0x0
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_XRES     0x1
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_YRES     0x2
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_BPP      0x3
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_ENABLE   0x4
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_BANK     0x5
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_VIRT_WIDTH              0x6
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_VIRT_HEIGHT             0x7
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_X_OFFSET                0x8
#define VIDEO_QEMU_VGA_VBE_DISPI_INDEX_Y_OFFSET                0x9

/* VBE_DISPI_INDEX_ID */
#define VIDEO_QEMU_VGA_VBE_DISPI_ID0                   0xB0C0
#define VIDEO_QEMU_VGA_VBE_DISPI_ID1                   0xB0C1
#define VIDEO_QEMU_VGA_VBE_DISPI_ID2                   0xB0C2
#define VIDEO_QEMU_VGA_VBE_DISPI_ID3                   0xB0C3
#define VIDEO_QEMU_VGA_VBE_DISPI_ID4                   0xB0C4
#define VVIDEO_QEMU_VGA_BE_DISPI_ID5                   0xB0C5

/* VBE_DISPI_INDEX_ENABLE */
#define VIDEO_QEMU_VGA_VBE_DISPI_DISABLED              0x00
#define VIDEO_QEMU_VGA_VBE_DISPI_ENABLED               0x01
#define VIDEO_QEMU_VGA_VBE_DISPI_GETCAPS               0x02
#define VIDEO_QEMU_VGA_VBE_DISPI_8BIT_DAC              0x20
#define VIDEO_QEMU_VGA_VBE_DISPI_LFB_ENABLED           0x40
#define VIDEO_QEMU_VGA_VBE_DISPI_NOCLEARMEM            0x80

int8_t video_qemu_vga_init(memory_heap_t* heap, const pci_dev_t* device);
int8_t video_qemu_vga_reinit(void);

#ifdef __cplusplus
}
#endif

#endif
