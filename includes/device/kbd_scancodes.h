/**
 * @file kbd_scancodes.h
 * @brief Keyboard scancodes header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___KBD_SCANCODES_H
#define ___KBD_SCANCODES_H 0

#include <types.h>
#include <device/kbd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KBD_PS2_EXTCODE_PREFIX    0xE0

#define KBD_SCANCODE_NULL         0x00
#define KBD_SCANCODE_ESC          0x01
#define KBD_SCANCODE_BACKSPACE    0x0E

#define KBD_SCANCODE_LEFTCTRL     0x1D
#define KBD_SCANCODE_RIGHTCTRL    0x61

#define KBD_SCANCODE_LEFTSHIFT    0x2A
#define KBD_SCANCODE_RIGHTSHIFT   0x36

#define KBD_SCANCODE_LEFTALT      0x38
#define KBD_SCANCODE_RIGHTALT     0x64

#define KBD_SCANCODE_CAPSLOCK     0x3A

#define KBD_SCANCODE_LEFTMETA     0x7D
#define KBD_SCANCODE_RIGHTMETA    0x7F

#define KBD_SCANCODE_F1           0x3B
#define KBD_SCANCODE_F2           0x3C
#define KBD_SCANCODE_F3           0x3D
#define KBD_SCANCODE_F4           0x3E
#define KBD_SCANCODE_F5           0x3F
#define KBD_SCANCODE_F6           0x40
#define KBD_SCANCODE_F7           0x41
#define KBD_SCANCODE_F8           0x42
#define KBD_SCANCODE_F9           0x43
#define KBD_SCANCODE_F10          0x44
#define KBD_SCANCODE_F11          0x57
#define KBD_SCANCODE_F12          0x58

#define KBD_SCANCODE_NUMLOCK      0x45
#define KBD_SCANCODE_SCROLLLOCK   0x46

#define KBD_KEYPAD_START          0x47
#define KBD_KEYPAD_END            0x53

#define KBD_SCANCODE_SYSRQ        0x63
#define KBD_SCANCODE_PRINTSCREEN  KBD_SCANCODE_SYSRQ
#define KBD_SCANCODE_BREAK        0x77
#define KBD_SCANCODE_PAUSE        KBD_SCANCODE_BREAK

#define KBD_SCANCODE_HOME         0x66
#define KBD_SCANCODE_UP           0x67
#define KBD_SCANCODE_PAGEUP       0x68
#define KBD_SCANCODE_LEFT         0x69
#define KBD_SCANCODE_RIGHT        0x6A
#define KBD_SCANCODE_END          0x6B
#define KBD_SCANCODE_DOWN         0x6C
#define KBD_SCANCODE_PAGEDOWN     0x6D
#define KBD_SCANCODE_INSERT       0x6E
#define KBD_SCANCODE_DELETE       0x6F

#define KBD_SCANCODE_SPACE        0x39
#define KBD_SCANCODE_KEYPAD_ENTER 0x60


extern const char16_t KBD_SCANCODES_NORMAL[];
extern const char16_t KBD_SCANCODES_CAPSON[];
extern const char16_t KBD_SCANCODES_SHIFT[];
extern const char16_t KBD_SCANCODES_SHIFTCAPSON[];
extern const char16_t KBD_SCANCODES_ALT[];
extern const char16_t KBD_SCANCODES_ALTSHIFT[];

char16_t kbd_scancode_get_value(char16_t scancode, kbd_state_t* ks, boolean_t* is_printable);
char16_t kbd_scancode_fixcode(char16_t ps2code);

#ifdef __cplusplus
}
#endif

#endif
