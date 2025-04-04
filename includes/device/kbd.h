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

#ifdef __cplusplus
extern "C" {
#endif

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_CMD_PORT 0x64

#define KBD_CMD_DISABLE_KBD_PORT 0xAD
#define KBD_CMD_DISABLE_MOUSE_PORT 0xA7

typedef struct kbd_state_t {
    boolean_t is_capson;
    boolean_t is_shift_pressed;
    boolean_t is_alt_pressed;
    boolean_t is_ctrl_pressed;
    boolean_t is_meta_pressed;
}kbd_state_t;

typedef struct kbd_report_t {
    kbd_state_t state;
    char16_t    key;
    boolean_t   is_pressed;
    boolean_t   is_printable;
}kbd_report_t;


_Static_assert(sizeof(kbd_report_t) == 10, "kbd_report_t size is not 10 bytes");

int8_t kbd_init(void);

int8_t kbd_handle_key(char16_t key, boolean_t pressed);

#ifdef __cplusplus
}
#endif

#endif
