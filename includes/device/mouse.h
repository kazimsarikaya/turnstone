/**
 * @file mouse.h
 * @brief mouse driver header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___MOUSE_H
#define ___MOUSE_H

#include <types.h>

#define MOUSE_BUTTON_LEFT_VIRTIO   0x110
#define MOUSE_BUTTON_RIGHT_VIRTIO  0x111
#define MOUSE_BUTTON_MIDDLE_VIRTIO 0x112

typedef enum mouse_buttons_t {
    MOUSE_BUTTON_LEFT   = 0x01,
    MOUSE_BUTTON_RIGHT  = 0x02,
    MOUSE_BUTTON_MIDDLE = 0x04
} mouse_buttons_t;

typedef struct mouse_report_t {
    mouse_buttons_t buttons;
    uint32_t        x;
    uint32_t        y;
    int32_t         wheel;
} mouse_report_t;

int8_t mouse_report(mouse_report_t * report);

#endif // ___MOUSE_H
