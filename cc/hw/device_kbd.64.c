/**
 * @file device_kbd.64.c
 * @brief Keyboard driver.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/kbd.h>
#include <device/mouse.h>
#include <logging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <apic.h>
#include <ports.h>
#include <memory/paging.h>
#include <strings.h>
#include <device/kbd_scancodes.h>
#include <acpi.h>
#include <time.h>
#include <shell.h>
#include <utils.h>
#include <graphics/screen.h>

MODULE("turnstone.kernel.hw.kbd");

boolean_t kbd_is_usb = false;

volatile char16_t kbd_ps2_tmp = NULL;
extern uint64_t shell_task_id;
extern uint64_t windowmanager_task_id;

kbd_state_t kbd_state = {0, 0, 0, 0, 0};

void video_text_print(const char_t* string);

int8_t dev_kbd_cleanup_isr(interrupt_frame_ext_t* frame);
int8_t dev_kbd_isr(interrupt_frame_ext_t* frame);

int8_t kbd_handle_key(char16_t key, boolean_t pressed){
    if(key == KBD_SCANCODE_CAPSLOCK && pressed == 0) {
        kbd_state.is_capson = !kbd_state.is_capson;
    }

    if(key == KBD_SCANCODE_LEFTSHIFT || key == KBD_SCANCODE_RIGHTSHIFT) {
        kbd_state.is_shift_pressed = pressed;
    }

    if(key == KBD_SCANCODE_LEFTALT || key == KBD_SCANCODE_RIGHTALT) {
        kbd_state.is_alt_pressed = pressed;
    }

    if(key == KBD_SCANCODE_LEFTCTRL || key == KBD_SCANCODE_RIGHTCTRL) {
        kbd_state.is_ctrl_pressed = pressed;
    }

    if(key == KBD_SCANCODE_LEFTMETA || key == KBD_SCANCODE_RIGHTMETA) {
        kbd_state.is_meta_pressed = pressed;
    }

    boolean_t is_printable = false;
    kbd_report_t report = {0};

    report.key = kbd_scancode_get_value(key, &kbd_state, &is_printable);
    report.is_pressed = pressed;
    report.is_printable = is_printable;
    report.state = kbd_state;

    if(shell_buffer != NULL) {
        buffer_append_bytes(shell_buffer, (uint8_t*)&report, sizeof(kbd_report_t));

        if(shell_task_id != 0) {
            task_set_interrupt_received(shell_task_id);
        } else if(windowmanager_task_id != 0) {
            task_set_interrupt_received(windowmanager_task_id);
        } else {
            video_text_print("no shell or wm\n");
        }
    }

    return 0;
}

int8_t dev_kbd_isr(interrupt_frame_ext_t* frame){
    UNUSED(frame);

    char16_t tmp_key = inb(KBD_DATA_PORT);

    if(tmp_key == KBD_PS2_EXTCODE_PREFIX) {
        kbd_ps2_tmp = tmp_key << 8;
    } else {
        kbd_ps2_tmp |= tmp_key;

        boolean_t is_pressed = (kbd_ps2_tmp & 0x80) != 0x80;

        kbd_ps2_tmp = kbd_scancode_fixcode(kbd_ps2_tmp);

        kbd_handle_key(kbd_ps2_tmp, is_pressed);

        kbd_ps2_tmp = NULL;
    }

    apic_eoi();

    return 0;
}

int8_t dev_kbd_cleanup_isr(interrupt_frame_ext_t* frame){
    UNUSED(frame);

    kbd_ps2_tmp = NULL;

    inb(KBD_DATA_PORT);

    apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_INTERRUPT_DISABLED);
    interrupt_irq_remove_handler(0x1, &dev_kbd_cleanup_isr);

    apic_eoi();

    return 0;
}

int8_t kbd_init(void){
    if(ACPI_CONTEXT->fadt->boot_architecture_flags & 2) {
        if(!kbd_is_usb) {
            PRINTLOG(KERNEL, LOG_INFO, "PS/2 keyboard found");
            interrupt_irq_set_handler(0x1, &dev_kbd_isr);
            apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
            apic_ioapic_enable_irq(0x1);

            return 0;
        }

        // disable PS/2 keyboard
        // it is ugly dirty hack

        kbd_ps2_tmp = 0xFFFF;

        interrupt_irq_set_handler(0x1, &dev_kbd_cleanup_isr);
        apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
        apic_ioapic_enable_irq(0x1);

        outb(KBD_CMD_PORT, KBD_CMD_DISABLE_KBD_PORT);
        outb(KBD_CMD_PORT, KBD_CMD_DISABLE_MOUSE_PORT);

        int32_t timeout = 1000000;

        while(kbd_ps2_tmp != 0 && timeout-- > 0) {
            asm volatile ("pause" : : : "memory");
        }

        if(timeout <= 0 && kbd_ps2_tmp != 0) {
            PRINTLOG(KERNEL, LOG_WARNING, "cannot disable PS/2 keyboard");
        }

        return 0;
    }

    return -1;
}
