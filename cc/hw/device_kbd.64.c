/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/kbd.h>
#include <logging.h>
#include <cpu.h>
#include <apic.h>
#include <ports.h>
#include <driver/virtio.h>
#include <driver/virtio_input.h>
#include <memory/paging.h>
#include <strings.h>
#include <device/kbd_scancodes.h>
#include <acpi.h>
#include <time.h>
#include <shell.h>

MODULE("turnstone.kernel.hw.kbd");

virtio_dev_t* virtio_kbd = NULL;
boolean_t kbd_is_usb = false;

wchar_t kbd_ps2_tmp = NULL;

kbd_state_t kbd_state = {0, 0, 0, 0, 0};

int8_t dev_virtio_kbd_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t dev_virtio_kbd_create_queues(virtio_dev_t* vdev);
int8_t dev_kbd_cleanup_isr(interrupt_frame_t* frame, uint8_t intnum);
int8_t dev_kbd_isr(interrupt_frame_t* frame, uint8_t intnum);

int8_t kbd_handle_key(wchar_t key, boolean_t pressed){
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


    if(pressed == 1) {
        wchar_t buffer[2] = {0, 0};
        boolean_t is_p = 0;

        buffer[0] = kbd_scancode_get_value(key, &kbd_state, &is_p);

        if(is_p) {
            char_t* data = wchar_to_char(buffer);

            if(shell_buffer != NULL) {
                buffer_append_byte(shell_buffer, data[0]);
            }

            memory_free(data);
        }
    }

    return 0;
}

int8_t dev_kbd_isr(interrupt_frame_t* frame, uint8_t intnum){
    UNUSED(frame);
    UNUSED(intnum);

    wchar_t tmp_key = inb(KBD_DATA_PORT);

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

int8_t dev_kbd_cleanup_isr(interrupt_frame_t* frame, uint8_t intnum){
    UNUSED(frame);
    UNUSED(intnum);

    kbd_ps2_tmp = NULL;

    inb(KBD_DATA_PORT);

    apic_eoi();

    return 0;
}

int8_t dev_virtio_kbd_isr(interrupt_frame_t* frame, uint8_t intnum){
    UNUSED(frame);
    UNUSED(intnum);

    virtio_queue_ext_t* vq_ev = &virtio_kbd->queues[0];
    virtio_queue_used_t* used = virtio_queue_get_used(virtio_kbd, vq_ev->vq);
    virtio_queue_avail_t* avail = virtio_queue_get_avail(virtio_kbd, vq_ev->vq);
    virtio_queue_descriptor_t* descs = virtio_queue_get_desc(virtio_kbd, vq_ev->vq);

    while(vq_ev->last_used_index < used->index) {
        uint16_t packet_desc_id = used->ring[vq_ev->last_used_index % virtio_kbd->queue_size].id;

        virtio_input_event_t* ev = (virtio_input_event_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(descs[packet_desc_id].address);

        if(ev->type == 1) {
            kbd_handle_key(ev->code, ev->value);
        }

        memory_memclean(ev, sizeof(virtio_input_event_t));

        descs[packet_desc_id].flags = VIRTIO_QUEUE_DESC_F_WRITE;

        avail->ring[avail->index % virtio_kbd->queue_size] = packet_desc_id;
        avail->index++;
        vq_ev->nd->vqn = 0;

        vq_ev->last_used_index++;
    }

    pci_msix_clear_pending_bit((pci_generic_device_t*)virtio_kbd->pci_dev->pci_header, virtio_kbd->msix_cap, 0);
    apic_eoi();

    return 0;
}

int8_t dev_virtio_kbd_create_queues(virtio_dev_t* vdev){
    vdev->queues = memory_malloc(sizeof(virtio_queue_ext_t) * 2);

    uint64_t item_size = sizeof(virtio_input_event_t);

    if(virtio_create_queue(vdev, 0, item_size, 1, 0, NULL, &dev_virtio_kbd_isr, NULL) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot create virtio keyboard queue");

        return -1;
    }

    return 0;
}


int8_t dev_virtio_kbd_init(void) {
    int8_t errors = 0;

    iterator_t* iter = linkedlist_iterator_create(PCI_CONTEXT->other_devices);
    const pci_dev_t* pci_dev = NULL;
    boolean_t is_found = false;

    while(iter->end_of_iterator(iter) != 0) {
        pci_dev = iter->get_item(iter);

        pci_common_header_t* pci_header = pci_dev->pci_header;

        if(pci_header->vendor_id == KBD_DEVICE_VENDOR_ID_VIRTIO && pci_header->device_id == KBD_DEVICE_DEVICE_ID_VIRTIO) {
            PRINTLOG(VIRTIO, LOG_INFO, "vendor: 0x%x, device: 0x%x", pci_header->vendor_id, pci_header->device_id);
            PRINTLOG(VIRTIO, LOG_INFO, "class: 0x%x, subclass: 0x%x", pci_header->class_code, pci_header->subclass_code);
            is_found = true;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(!is_found) {
        PRINTLOG(VIRTIO, LOG_ERROR, "virtkbd device not found");

        return -1;
    }

    PRINTLOG(VIRTIO, LOG_INFO, "virtkbd device found starting");

    virtio_kbd = virtio_get_device(pci_dev);

    if(virtio_kbd == NULL) {

        return -1;
    }

    virtio_kbd->queue_size = VIRTIO_QUEUE_SIZE;

    if(virtio_init_dev(virtio_kbd, NULL, &dev_virtio_kbd_create_queues) != 0) {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot init virtio keyboard");

        errors = -1;
    } else {
        PRINTLOG(VIRTIO, LOG_INFO, "virtkbd device started");
    }

    return errors;
}

int8_t kbd_init(void){
    boolean_t virtual_kbd = false;
    if(dev_virtio_kbd_init() == 0) {
        virtual_kbd = true;
    }

    if(ACPI_CONTEXT->fadt->boot_architecture_flags & 2) {
        if(!virtual_kbd && !kbd_is_usb) {
            PRINTLOG(KERNEL, LOG_INFO, "PS/2 keyboard found");
            interrupt_irq_set_handler(0x1, &dev_kbd_isr);
            apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
            apic_ioapic_enable_irq(0x1);

            return 0;
        }

        kbd_ps2_tmp = 0xFFFF;

        interrupt_irq_set_handler(0x1, &dev_kbd_cleanup_isr);
        apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
        apic_ioapic_enable_irq(0x1);

        outb(KBD_CMD_PORT, KBD_CMD_DISABLE_KBD_PORT);
        outb(KBD_CMD_PORT, KBD_CMD_DISABLE_MOUSE_PORT);

        while(kbd_ps2_tmp != 0) {
            asm volatile ("pause" : : : "memory");
        }

        apic_ioapic_setup_irq(0x1, APIC_IOAPIC_TRIGGER_MODE_EDGE);
        apic_ioapic_disable_irq(0x1);
        interrupt_irq_remove_handler(0x1, &dev_kbd_cleanup_isr);

        return 0;
    }

    return -1;
}
