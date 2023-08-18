/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <device/kbd_scancodes.h>

MODULE("turnstone.kernel.hw.kbd");

const wchar_t KBD_SCANCODES_NORMAL[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KBD_SCANCODE_BACKSPACE /*0x0e*/,
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KBD_SCANCODE_RIGHTSHIFT,
    '*', KBD_SCANCODE_LEFTALT, ' ', KBD_SCANCODE_CAPSLOCK,
    KBD_SCANCODE_F1, KBD_SCANCODE_F2, KBD_SCANCODE_F3, KBD_SCANCODE_F4, KBD_SCANCODE_F5,
    KBD_SCANCODE_F6, KBD_SCANCODE_F7, KBD_SCANCODE_F8, KBD_SCANCODE_F9, KBD_SCANCODE_F10,
    /*keypad start*/ KBD_SCANCODE_NUMLOCK, KBD_SCANCODE_SCROLLLOCK, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.' /*keypad end 0x53*/,
    KBD_SCANCODE_NULL /*0x54*/, KBD_SCANCODE_NULL /*0x55*/, KBD_SCANCODE_NULL /*0x56*/,
    KBD_SCANCODE_F11, KBD_SCANCODE_F12,
    KBD_SCANCODE_NULL /*0x59*/, KBD_SCANCODE_NULL /*0x5a*/, KBD_SCANCODE_NULL /*0x5b*/, KBD_SCANCODE_NULL /*0x5c*/, KBD_SCANCODE_NULL /*0x5d*/, KBD_SCANCODE_NULL /*0x5e*/,
    KBD_SCANCODE_NULL /*0x5f*/, '\n' /*keypad enter 0x60*/, KBD_SCANCODE_RIGHTTCTRL, '/' /*0x62 keypad slash*/, KBD_SCANCODE_NULL /*0x63*/,
    KBD_SCANCODE_RIGHTALT, '\n' /*0x65 linefeed*/,
    KBD_SCANCODE_HOME, KBD_SCANCODE_UP, KBD_SCANCODE_PAGEUP, KBD_SCANCODE_LEFT, KBD_SCANCODE_RIGHT, KBD_SCANCODE_END, KBD_SCANCODE_DOWN, KBD_SCANCODE_PAGEDOWN, KBD_SCANCODE_INSERT, KBD_SCANCODE_DELETE,
    KBD_SCANCODE_NULL /*0x70*/, KBD_SCANCODE_NULL /*0x71*/, KBD_SCANCODE_NULL /*0x72*/, KBD_SCANCODE_NULL /*0x73*/, KBD_SCANCODE_NULL /*0x74*/, KBD_SCANCODE_NULL /*0x75*/,
    KBD_SCANCODE_NULL /*0x76*/, KBD_SCANCODE_NULL /*0x77*/, KBD_SCANCODE_NULL /*0x78*/, KBD_SCANCODE_NULL /*0x79*/, KBD_SCANCODE_NULL /*0x7a*/, KBD_SCANCODE_NULL /*0x7b*/, KBD_SCANCODE_NULL /*0x7c*/,
    KBD_SCANCODE_LEFTMETA, KBD_SCANCODE_RIGHTMETA
};

#define KBD_SCANCODES_NORMAL_SIZE (sizeof(KBD_SCANCODES_NORMAL) / sizeof(wchar_t))

const wchar_t KBD_SCANCODES_CAPSON[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KBD_SCANCODE_BACKSPACE,
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, '\\', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', KBD_SCANCODE_RIGHTSHIFT,
};

#define KBD_SCANCODES_CAPSON_SIZE (sizeof(KBD_SCANCODES_CAPSON) / sizeof(wchar_t))

const wchar_t KBD_SCANCODES_SHIFT[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', KBD_SCANCODE_BACKSPACE,
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', KBD_SCANCODE_RIGHTSHIFT,
};

#define KBD_SCANCODES_SHIFT_SIZE (sizeof(KBD_SCANCODES_SHIFT) / sizeof(wchar_t))

const wchar_t KBD_SCANCODES_SHIFTCAPSON[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', KBD_SCANCODE_BACKSPACE,
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, '|', 'z', 'x', 'c', 'v', 'b', 'n', 'm', '<', '>', '?', KBD_SCANCODE_RIGHTSHIFT,
};

#define KBD_SCANCODES_SHIFTCAPSON_SIZE (sizeof(KBD_SCANCODES_SHIFTCAPSON) / sizeof(wchar_t))

const wchar_t KBD_SCANCODES_ALT[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, L'¡', L'™', L'£', L'¢', L'∞', L'§', L'¶', L'•', L'ª', L'º', L'–', L'≠', KBD_SCANCODE_BACKSPACE,
    '\t', L'œ', L'´', L'´', L'®', L'†', L'¥', L'¨', L'ˆ', L'ø', L'π', L'“', L'‘', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, L'å', L'ß', L'∂', L'ƒ', L'©', L'˙', L'∆', L'˚', L'¬', L'…', L'æ', L'`' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, L'«', L'Ω', L'≈', KBD_SCANCODE_NULL /*empty*/, L'√', L'∫', L'˜', L'µ', L'≤', L'≥', L'÷', KBD_SCANCODE_RIGHTSHIFT,
};

#define KBD_SCANCODES_ALT_SIZE (sizeof(KBD_SCANCODES_ALT) / sizeof(wchar_t))

const wchar_t KBD_SCANCODES_ALTSHIFT[] = {
    KBD_SCANCODE_NULL, KBD_SCANCODE_ESC, L'⁄', L'€', L'‹', L'›', L'ﬁ', L'ﬂ', L'‡', L'°', L'·', L'‚', L'—', L'±', KBD_SCANCODE_BACKSPACE,
    '\t', L'Œ', L'„', L'´', L'‰', L'ˇ', L'Á', L'¨', L'ˆ', L'Ø', L'∏', L'”', L'’', '\n' /* enter 0x1c*/,
    KBD_SCANCODE_LEFTCTRL, L'Å', L'Í', L'Î', L'Ï', L'˝', L'Ó', L'Ô', L'', L'Ò', L'Ú', L'Æ', L'`' /*0x29*/,
    KBD_SCANCODE_LEFTSHIFT, L'»', L'¸', L'˛', L'Ç', L'◊', L'ı', L'˜', L'Â', L'¯', L'˘', L'¿', KBD_SCANCODE_RIGHTSHIFT,
};

#define KBD_SCANCODES_ALTSHIFT_SIZE (sizeof(KBD_SCANCODES_ALTSHIFT) / sizeof(wchar_t))

static inline boolean_t kbd_scancode_is_printable(wchar_t scancode){
    return ((scancode > KBD_SCANCODE_ESC && scancode < KBD_SCANCODE_BACKSPACE) ||
            (scancode > KBD_SCANCODE_BACKSPACE && scancode < KBD_SCANCODE_LEFTCTRL) ||
            (scancode > KBD_SCANCODE_LEFTCTRL && scancode < KBD_SCANCODE_LEFTSHIFT) ||
            (scancode > KBD_SCANCODE_LEFTSHIFT && scancode < KBD_SCANCODE_RIGHTSHIFT) ||
            (scancode == KBD_SCANCODE_SPACE) ||
            (scancode == KBD_SCANCODE_KEYPAD_ENTER) ||
            (scancode >= KBD_KEYPAD_START && scancode <= KBD_KEYPAD_END));
}

wchar_t kbd_scancode_get_value(wchar_t scancode, kbd_state_t* ks, boolean_t* is_printable){

    if(is_printable) {
        *is_printable = kbd_scancode_is_printable(scancode);
    }

    if(ks->is_capson) {
        if(ks->is_shift_pressed) {
            if(scancode < KBD_SCANCODES_SHIFTCAPSON_SIZE) {
                return KBD_SCANCODES_SHIFTCAPSON[scancode];
            }
        } else {
            if(scancode < KBD_SCANCODES_CAPSON_SIZE) {
                return KBD_SCANCODES_CAPSON[scancode];
            }
        }
    } else if(ks->is_alt_pressed) {
        if(ks->is_shift_pressed) {
            if(scancode < KBD_SCANCODES_ALTSHIFT_SIZE) {
                return KBD_SCANCODES_ALTSHIFT[scancode];
            }
        } else {
            if(scancode < KBD_SCANCODES_ALT_SIZE) {
                return KBD_SCANCODES_ALT[scancode];
            }
        }
    } else if(ks->is_shift_pressed) {
        if(scancode < KBD_SCANCODES_SHIFT_SIZE) {
            return KBD_SCANCODES_SHIFT[scancode];
        }
    }

    if(scancode >= KBD_SCANCODES_NORMAL_SIZE) {
        return KBD_SCANCODE_NULL;
    }

    return KBD_SCANCODES_NORMAL[scancode];
}

wchar_t kbd_scancode_fixcode(wchar_t ps2code){
    if(((ps2code >> 8) & KBD_PS2_EXTCODE_PREFIX) == KBD_PS2_EXTCODE_PREFIX) {
        ps2code &= 0x7F; // msb bit is about press or release

        switch (ps2code) {
        case 0x1D:
            return KBD_SCANCODE_RIGHTTCTRL;
        case 0x38:
            return KBD_SCANCODE_RIGHTALT;
        case 0x47:
            return KBD_SCANCODE_HOME;
        case 0x48:
            return KBD_SCANCODE_UP;
        case 0x49:
            return KBD_SCANCODE_PAGEUP;
        case 0x4B:
            return KBD_SCANCODE_LEFT;
        case 0x4D:
            return KBD_SCANCODE_RIGHT;
        case 0x4F:
            return KBD_SCANCODE_END;
        case 0x50:
            return KBD_SCANCODE_DOWN;
        case 0x51:
            return KBD_SCANCODE_PAGEDOWN;
        case 0x52:
            return KBD_SCANCODE_INSERT;
        case 0x53:
            return KBD_SCANCODE_DELETE;
        case 0x5B:
            return KBD_SCANCODE_LEFTMETA;
        case 0x5C:
            return KBD_SCANCODE_RIGHTMETA;
        default:
            return KBD_SCANCODE_NULL;
        }

    }

    ps2code &= 0x7F;

    switch (ps2code) {
    case 0x56:
        return 0x29;
    default:
        break;
    }

    return ps2code;
}
