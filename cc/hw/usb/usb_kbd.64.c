/**
 * @file usb_kbd.64.c
 * @brief USB keyboard driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <memory.h>
#include <logging.h>
#include <device/kbd.h>
#include <device/kbd_scancodes.h>
#include <utils.h>

MODULE("turnstone.kernel.hw.usb.kbd");

extern boolean_t kbd_is_usb;

typedef struct usb_kbd_report_t {
    struct {
        boolean_t left_ctrl   : 1;
        boolean_t left_shift  : 1;
        boolean_t left_alt    : 1;
        boolean_t left_meta   : 1;
        boolean_t right_ctrl  : 1;
        boolean_t right_shift : 1;
        boolean_t right_alt   : 1;
        boolean_t right_meta  : 1;
    } __attribute__((packed)) modifiers;
    uint8_t reserved;
    uint8_t key[6];
} __attribute__((packed)) usb_kbd_report_t;


typedef struct usb_driver_t {
    usb_device_t*    usb_device;
    usb_kbd_report_t old_usb_kbd_report;
    usb_kbd_report_t new_usb_kbd_report;
    usb_transfer_t*  usb_transfer;
} usb_driver_t;

// we need to map usb keyboard to scancodes to ev codes here a char16_t array
const char16_t KBD_USB_SCANCODE_MAP[] = {
    KBD_SCANCODE_NULL, // 0x00
    KBD_SCANCODE_NULL, // 0x01
    KBD_SCANCODE_NULL, // 0x02
    KBD_SCANCODE_NULL, // 0x03
    0x1e, // 0x04 A
    0x30, // 0x05 B
    0x2e, // 0x06 C
    0x20, // 0x07 D
    0x12, // 0x08 E
    0x21, // 0x09 F
    0x22, // 0x0a G
    0x23, // 0x0b H
    0x17, // 0x0c I
    0x24, // 0x0d J
    0x25, // 0x0e K
    0x26, // 0x0f L
    0x32, // 0x10 M
    0x31, // 0x11 N
    0x18, // 0x12 O
    0x19, // 0x13 P
    0x10, // 0x14 Q
    0x13, // 0x15 R
    0x1f, // 0x16 S
    0x14, // 0x17 T
    0x16, // 0x18 U
    0x2f, // 0x19 V
    0x11, // 0x1a W
    0x2d, // 0x1b X
    0x15, // 0x1c Y
    0x2c, // 0x1d Z
    0x02, // 0x1e 1
    0x03, // 0x1f 2
    0x04, // 0x20 3
    0x05, // 0x21 4
    0x06, // 0x22 5
    0x07, // 0x23 6
    0x08, // 0x24 7
    0x09, // 0x25 8
    0x0a, // 0x26 9
    0x0b, // 0x27 0
    0x1c, // 0x28 Enter
    KBD_SCANCODE_ESC, // 0x29 Escape
    KBD_SCANCODE_BACKSPACE, // 0x2a Backspace
    0x0f, // 0x2b Tab
    KBD_SCANCODE_SPACE, // 0x2c Space
    0x0c, // 0x2d -
    0x0d, // 0x2e =
    0x1a, // 0x2f [
    0x1b, // 0x30 ]
    0x2b, // 0x31 Backslash
    KBD_SCANCODE_NULL, // 0x32 Non-US #
    0x27, // 0x33 ;
    0x28, // 0x34 '
    0x29, // 0x35 `
    0x33, // 0x36 ,
    0x34, // 0x37 .
    0x35, // 0x38 /
    0x3a, // 0x39 Caps Lock
    KBD_SCANCODE_F1, // 0x3a F1
    KBD_SCANCODE_F2, // 0x3b F2
    KBD_SCANCODE_F3, // 0x3c F3
    KBD_SCANCODE_F4, // 0x3d F4
    KBD_SCANCODE_F5, // 0x3e F5
    KBD_SCANCODE_F6, // 0x3f F6
    KBD_SCANCODE_F7, // 0x40 F7
    KBD_SCANCODE_F8, // 0x41 F8
    KBD_SCANCODE_F9, // 0x42 F9
    KBD_SCANCODE_F10, // 0x43 F10
    KBD_SCANCODE_F11, // 0x44 F11
    KBD_SCANCODE_F12, // 0x45 F12
    KBD_SCANCODE_PRINTSCREEN, // 0x46 Print Screen
    KBD_SCANCODE_SCROLLLOCK, // 0x47 Scroll Lock
    KBD_SCANCODE_PAUSE, // 0x48 Pause
    KBD_SCANCODE_INSERT, // 0x49 Insert
    KBD_SCANCODE_HOME, // 0x4a Home
    KBD_SCANCODE_PAGEUP, // 0x4b Page Up
    KBD_SCANCODE_DELETE, // 0x4c Delete
    KBD_SCANCODE_END, // 0x4d End
    KBD_SCANCODE_PAGEDOWN, // 0x4e Page Down
    KBD_SCANCODE_RIGHT, // 0x4f Right Arrow
    KBD_SCANCODE_LEFT, // 0x50 Left Arrow
    KBD_SCANCODE_DOWN, // 0x51 Down Arrow
    KBD_SCANCODE_UP, // 0x52 Up Arrow
    KBD_SCANCODE_NUMLOCK, // 0x53 Num Lock
    0x35, // 0x54 Keypad /
    0x37, // 0x55 Keypad *
    0x4a, // 0x56 Keypad -
    0x4e, // 0x57 Keypad +
    KBD_SCANCODE_KEYPAD_ENTER, // 0x58 Keypad Enter
    0x4f, // 0x59 Keypad 1
    0x50, // 0x5a Keypad 2
    0x51, // 0x5b Keypad 3
    0x4b, // 0x5c Keypad 4
    0x4c, // 0x5d Keypad 5
    0x4d, // 0x5e Keypad 6
    0x47, // 0x5f Keypad 7
    0x48, // 0x60 Keypad 8
    0x49, // 0x61 Keypad 9
    0x52, // 0x62 Keypad 0
    0x53, // 0x63 Keypad .
    0x56, // 0x64 Non-US \|
    0x62, // 0x65 Application
    0x65, // 0x66 Power
    0x66, // 0x67 Keypad =
    0x67, // 0x68 F13
    0x68, // 0x69 F14
    0x69, // 0x6a F15
    0x6a, // 0x6b F16
    0x6b, // 0x6c F17
    0x6c, // 0x6d F18
    0x6d, // 0x6e F19
    0x6e, // 0x6f F20
    0x6f, // 0x70 F21
    0x70, // 0x71 F22
    0x71, // 0x72 F23
    0x72, // 0x73 F24
    0x73, // 0x74 Execute
    0x74, // 0x75 Help
    0x75, // 0x76 Menu
    0x76, // 0x77 Select
    0x77, // 0x78 Stop
    0x78, // 0x79 Again
    0x79, // 0x7a Undo
    0x7a, // 0x7b Cut
    0x7b, // 0x7c Copy
    0x7c, // 0x7d Paste
    0x7d, // 0x7e Find
    0x7e, // 0x7f Mute
    0x7f, // 0x80 Volume Up
    0x80, // 0x81 Volume Down
    0x81, // 0x82 Locking Caps Lock
    0x82, // 0x83 Locking Num Lock
    0x83, // 0x84 Locking Scroll Lock
    0x84, // 0x85 Keypad Comma
    0x85, // 0x86 Keypad Equal Sign
    0x86, // 0x87 International1
    0x87, // 0x88 International2
    0x88, // 0x89 International3
    0x89, // 0x8a International4
    0x8a, // 0x8b International5
    0x8b, // 0x8c International6
    0x8c, // 0x8d International7
    0x8d, // 0x8e International8
    0x8e, // 0x8f International9
    0x8f, // 0x90 LANG1
    0x90, // 0x91 LANG2
    0x91, // 0x92 LANG3
    0x92, // 0x93 LANG4
    0x93, // 0x94 LANG5
    0x94, // 0x95 LANG6
    0x95, // 0x96 LANG7
    0x96, // 0x97 LANG8
    0x97, // 0x98 LANG9
    0x98, // 0x99 Alternate Erase
    0x99, // 0x9a SysReq/Attention
    0x9a, // 0x9b Cancel
    0x9b, // 0x9c Clear
    0x9c, // 0x9d Prior
    0x9d, // 0x9e Return
    0x9e, // 0x9f Separator
    0x9f, // 0xa0 Out
    0xa0, // 0xa1 Oper
    0xa1, // 0xa2 Clear/Again
    0xa2, // 0xa3 CrSel/Props
    0xa3, // 0xa4 ExSel
    0xa4, // 0xa5 Reserved
    0xa5, // 0xa6 Reserved
    0xa6, // 0xa7 Reserved
    0xa7, // 0xa8 Reserved
    0xa8, // 0xa9 Reserved
    0xa9, // 0xaa Reserved
    0xaa, // 0xab Reserved
    0xab, // 0xac Reserved
    0xac, // 0xad Reserved
    0xad, // 0xae Reserved
    0xae, // 0xaf Reserved
    0xaf, // 0xb0 Keypad 00
    0xb0, // 0xb1 Keypad 000
    0xb1, // 0xb2 Thousands Separator
    0xb2, // 0xb3 Decimal Separator
    0xb3, // 0xb4 Currency Unit
    0xb4, // 0xb5 Currency Sub-unit
    0xb5, // 0xb6 Keypad (
    0xb6, // 0xb7 Keypad )
    0xb7, // 0xb8 Keypad {
    0xb8, // 0xb9 Keypad }
    0xb9, // 0xba Keypad Tab
    0xba, // 0xbb Keypad Backspace
    0xbb, // 0xbc Keypad A
    0xbc, // 0xbd Keypad B
    0xbd, // 0xbe Keypad C
    0xbe, // 0xbf Keypad D
    0xbf, // 0xc0 Keypad E
    0xc0, // 0xc1 Keypad F
    0xc1, // 0xc2 Keypad XOR
    0xc2, // 0xc3 Keypad ^
    0xc3, // 0xc4 Keypad %
    0xc4, // 0xc5 Keypad <
    0xc5, // 0xc6 Keypad >
    0xc6, // 0xc7 Keypad &
    0xc7, // 0xc8 Keypad &&
    0xc8, // 0xc9 Keypad |
    0xc9, // 0xca Keypad ||
    0xca, // 0xcb Keypad :
    0xcb, // 0xcc Keypad #
    0xcc, // 0xcd Keypad Space
    0xcd, // 0xce Keypad @
    0xce, // 0xcf Keypad !
    0xcf, // 0xd0 Keypad Memory Store
    0xd0, // 0xd1 Keypad Memory Recall
    0xd1, // 0xd2 Keypad Memory Clear
    0xd2, // 0xd3 Keypad Memory Add
    0xd3, // 0xd4 Keypad Memory Subtract
    0xd4, // 0xd5 Keypad Memory Multiply
    0xd5, // 0xd6 Keypad Memory Divide
    0xd6, // 0xd7 Keypad +/-
    0xd7, // 0xd8 Keypad Clear
    0xd8, // 0xd9 Keypad Clear Entry
    0xd9, // 0xda Keypad Binary
    0xda, // 0xdb Keypad Octal
    0xdb, // 0xdc Keypad Decimal
    0xdc, // 0xdd Keypad Hexadecimal
    0xdd, // 0xde Reserved
    0xde, // 0xdf Reserved
    KBD_SCANCODE_LEFTCTRL, // 0xe0 Left Control
    KBD_SCANCODE_LEFTSHIFT, // 0xe1 Left Shift
    KBD_SCANCODE_LEFTALT, // 0xe2 Left Alt
    0xe2, // 0xe3 Left GUI
    KBD_SCANCODE_RIGHTCTRL, // 0xe4 Right Control
    KBD_SCANCODE_RIGHTSHIFT, // 0xe5 Right Shift
    KBD_SCANCODE_RIGHTALT, // 0xe6 Right Alt
    0xe6, // 0xe7 Right GUI
    0xe7, // 0xe8 Reserved
    0xe8, // 0xe9 Reserved
    0xe9, // 0xea Reserved
    0xea, // 0xeb Reserved
    0xeb, // 0xec Reserved
    0xec, // 0xed Reserved
    0xed, // 0xee Reserved
    0xee, // 0xef Reserved
    0xef, // 0xf0 Media Play/Pause
    0xf0, // 0xf1 Media Stop CD
    0xf1, // 0xf2 Media Previous Song
    0xf2, // 0xf3 Media Next Song
    0xf3, // 0xf4 Media Eject CD
    0xf4, // 0xf5 Media Volume Up
    0xf5, // 0xf6 Media Volume Down
    0xf6, // 0xf7 Media Mute
    0xf7, // 0xf8 Media WWW
    0xf8, // 0xf9 Media Back
    0xf9, // 0xfa Media Forward
    0xfa, // 0xfb Media Stop
    0xfb, // 0xfc Media Find
    0xfc, // 0xfd Media Scroll Up
    0xfd, // 0xfe Media Scroll Down
    0xfe, // 0xff Media Edit
    0xff, // 0x100 Media Sleep
};

int8_t usb_keyboard_transfer_cb(usb_controller_t* usb_controller, usb_transfer_t* usb_transfer);


int8_t usb_keyboard_transfer_cb(usb_controller_t* usb_controller, usb_transfer_t* usb_transfer) {
    usb_driver_t* usb_keyboard = usb_transfer->device->driver;


    if(usb_transfer->success) {
        boolean_t phantoms = false;

        for(uint8_t i = 0; i < 6; i++) {
            if(usb_keyboard->new_usb_kbd_report.key[i] == 1) {
                phantoms = true;
            }
        }

        if(phantoms) {
            memory_memclean(&usb_keyboard->new_usb_kbd_report, sizeof(usb_kbd_report_t));
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.left_alt && !usb_keyboard->old_usb_kbd_report.modifiers.left_alt) {
            kbd_handle_key(KBD_SCANCODE_LEFTALT, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.left_alt && usb_keyboard->old_usb_kbd_report.modifiers.left_alt) {
            kbd_handle_key(KBD_SCANCODE_LEFTALT, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.right_alt && !usb_keyboard->old_usb_kbd_report.modifiers.right_alt) {
            kbd_handle_key(KBD_SCANCODE_RIGHTALT, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.right_alt && usb_keyboard->old_usb_kbd_report.modifiers.right_alt) {
            kbd_handle_key(KBD_SCANCODE_RIGHTALT, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.left_ctrl && !usb_keyboard->old_usb_kbd_report.modifiers.left_ctrl) {
            kbd_handle_key(KBD_SCANCODE_LEFTCTRL, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.left_ctrl && usb_keyboard->old_usb_kbd_report.modifiers.left_ctrl) {
            kbd_handle_key(KBD_SCANCODE_LEFTCTRL, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.right_ctrl && !usb_keyboard->old_usb_kbd_report.modifiers.right_ctrl) {
            kbd_handle_key(KBD_SCANCODE_RIGHTCTRL, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.right_ctrl && usb_keyboard->old_usb_kbd_report.modifiers.right_ctrl) {
            kbd_handle_key(KBD_SCANCODE_RIGHTCTRL, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.left_shift && !usb_keyboard->old_usb_kbd_report.modifiers.left_shift) {
            kbd_handle_key(KBD_SCANCODE_LEFTSHIFT, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.left_shift && usb_keyboard->old_usb_kbd_report.modifiers.left_shift) {
            kbd_handle_key(KBD_SCANCODE_LEFTSHIFT, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.right_shift && !usb_keyboard->old_usb_kbd_report.modifiers.right_shift) {
            kbd_handle_key(KBD_SCANCODE_RIGHTSHIFT, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.right_shift && usb_keyboard->old_usb_kbd_report.modifiers.right_shift) {
            kbd_handle_key(KBD_SCANCODE_RIGHTSHIFT, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.left_meta && !usb_keyboard->old_usb_kbd_report.modifiers.left_meta) {
            kbd_handle_key(KBD_SCANCODE_LEFTMETA, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.left_meta && usb_keyboard->old_usb_kbd_report.modifiers.left_meta) {
            kbd_handle_key(KBD_SCANCODE_LEFTMETA, false);
        }

        if(usb_keyboard->new_usb_kbd_report.modifiers.right_meta && !usb_keyboard->old_usb_kbd_report.modifiers.right_meta) {
            kbd_handle_key(KBD_SCANCODE_RIGHTMETA, true);
        }

        if(!usb_keyboard->new_usb_kbd_report.modifiers.right_meta && usb_keyboard->old_usb_kbd_report.modifiers.right_meta) {
            kbd_handle_key(KBD_SCANCODE_RIGHTMETA, false);
        }




        for(uint8_t i = 0; i < 6; i++) {
            if(usb_keyboard->old_usb_kbd_report.key[i] == 0) {
                continue;
            }

            boolean_t found = false;

            for(uint8_t j = 0; j < 6; j++) {
                if(usb_keyboard->old_usb_kbd_report.key[i] == usb_keyboard->new_usb_kbd_report.key[j]) {
                    found = true;
                }
            }

            if(!found) {
                // kbd_release_key(usb_keyboard->old_usb_kbd_report.key[i]);
                char16_t key = usb_keyboard->old_usb_kbd_report.key[i];
                PRINTLOG(USB, LOG_INFO, "key released: %x", key);
                key = KBD_USB_SCANCODE_MAP[key];
                PRINTLOG(USB, LOG_INFO, "key mapped: %x", key);
                kbd_handle_key(key, false);
            }
        }

        for(uint8_t i = 0; i < 6; i++) {
            if(usb_keyboard->new_usb_kbd_report.key[i] == 0) {
                continue;
            }

            boolean_t found = false;

            for(uint8_t j = 0; j < 6; j++) {
                if(usb_keyboard->new_usb_kbd_report.key[i] == usb_keyboard->old_usb_kbd_report.key[j]) {
                    found = true;
                }
            }

            if(!found) {
                // kbd_release_key(usb_keyboard->old_usb_kbd_report.key[i]);
                char16_t key = usb_keyboard->new_usb_kbd_report.key[i];
                PRINTLOG(USB, LOG_INFO, "key pressed: %x", key);
                key = KBD_USB_SCANCODE_MAP[key];
                PRINTLOG(USB, LOG_INFO, "key mapped: %x", key);
                kbd_handle_key(key, true);
            }
        }

        memory_memcopy(&usb_keyboard->new_usb_kbd_report, &usb_keyboard->old_usb_kbd_report, sizeof(usb_kbd_report_t));
    } else {
        PRINTLOG(USB, LOG_ERROR, "transfer failed");
    }



    memory_memclean(&usb_keyboard->new_usb_kbd_report, sizeof(usb_kbd_report_t));
    usb_transfer->complete = false;
    usb_transfer->success = false;

    if(usb_controller->bulk_transfer(usb_controller, usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");

        return -1;
    }

    return 0;
}


int8_t usb_keyboard_init(usb_device_t* usb_device) {
    usb_driver_t* usb_keyboard = memory_malloc(sizeof(usb_driver_t));

    if(!usb_keyboard) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb keyboard");

        return -1;
    }

    usb_keyboard->usb_device = usb_device;
    usb_device->driver = usb_keyboard;

    usb_keyboard->usb_transfer = memory_malloc(sizeof(usb_transfer_t));

    if(!usb_keyboard->usb_transfer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb transfer");
        memory_free(usb_keyboard);

        return -1;
    }

    usb_keyboard->usb_transfer->device = usb_device;
    usb_keyboard->usb_transfer->endpoint = usb_device->configurations[usb_device->selected_config]->endpoints[0];
    usb_keyboard->usb_transfer->transfer_callback = usb_keyboard_transfer_cb;


    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_IDLE,
                            0, usb_device->configurations[usb_device->selected_config]->interface->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set idle");
        memory_free(usb_keyboard->usb_transfer);
        memory_free(usb_keyboard);

        return -1;
    }

    usb_keyboard->usb_transfer->data = (uint8_t*)&usb_keyboard->new_usb_kbd_report;
    usb_keyboard->usb_transfer->length = sizeof(usb_kbd_report_t);



    if(usb_device->controller->bulk_transfer(usb_device->controller, usb_keyboard->usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");
        memory_free(usb_keyboard->usb_transfer);
        memory_free(usb_keyboard);

        return -1;
    }

    kbd_is_usb = true;

    PRINTLOG(USB, LOG_INFO, "usb keyboard initialized");

    return 0;
}
