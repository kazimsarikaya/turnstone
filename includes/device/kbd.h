/**
 * @file kbd.h
 * @brief keyboard device interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___DEVICE_KBD_H
/*! prevent duplicate header error macro */
#define ___DEVICE_KBD_H 0


#include <types.h>
#include <cpu/interrupt.h>
#include <pci.h>
#include <driver/virtio.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT 0x64

#define KBD_CMD_DISABLE_KBD_PORT 0xAD
#define KBD_CMD_DISABLE_MOUSE_PORT 0xA7

#define KBD_DEVICE_VENDOR_ID_VIRTIO  0x1AF4
#define KBD_DEVICE_DEVICE_ID_VIRTIO  0x1052

int8_t kbd_init(void);

int8_t dev_virtio_kbd_init(void);
int8_t kbd_handle_key(wchar_t key, boolean_t pressed);
#endif
