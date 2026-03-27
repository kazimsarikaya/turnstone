/**
 * @file video_init.64.c
 * @brief Video Initialization
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <list.h>
#include <pci.h>
#include <driver/video.h>
#include <driver/video_vmwaresvga.h>
#include <driver/video_qemuvga.h>
#include <logging.h>
#include <apic.h>
#include <ports.h>
#include <graphics/text_cursor.h>
#include <cpu/sync.h>
#include <cpu/cpu_state.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.video");

static boolean_t GRAPHICS_MODE = false;

static lock_t* video_lock = NULL;

video_graphics_print_f VIDEO_GRAPHICS_PRINT = NULL;

void video_set_graphics_mode(boolean_t enabled) {
    GRAPHICS_MODE = enabled;
}

boolean_t video_is_graphics_mode(void) {
    return GRAPHICS_MODE;
}

int8_t video_configure_lock(void) {
    if(video_lock == NULL) {
        video_lock = lock_create();

        if(video_lock == NULL) {
            return -1;
        }
    }

    video_text_print("video init done\n");
    char_t* dbg = strprintf("video lock address: 0x%p\n", video_lock);
    video_text_print(dbg);
    memory_free(dbg);

    return 0;
}

void video_acquire_lock(void) {
    if(video_lock) {
        lock_acquire(video_lock);
    }
}

void video_release_lock(void) {
    if(video_lock) {
        lock_release(video_lock);
    }
}

void video_print(const char_t* string) {
    lock_acquire(video_lock);

    video_text_print(string);

    if(GRAPHICS_MODE) {
        VIDEO_GRAPHICS_PRINT(string);
    }

    lock_release(video_lock);
}

static const int32_t serial_ports[] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};

void video_text_print(const char_t* string) {
    if(string == NULL) {
        return;
    }

    uint32_t apic_id = cpu_state->local_apic_id;

    if(apic_id >= ARRAY_SIZE(serial_ports)) {
        return;
    }

    size_t i = 0;

    while(string[i] != '\0') {
        write_serial(serial_ports[apic_id], string[i]);
        i++;
    }
}

int8_t video_display_init(memory_heap_t* heap, list_t* display_controllers) {
    iterator_t* iter = list_iterator_create(display_controllers);

    if(iter == NULL) {
        return -1;
    }

    while(!iter->end_of_iterator(iter)) {
        const pci_dev_t* device = iter->get_item(iter);

        if(device->pci_header->common.vendor_id == VIDEO_PCI_DEVICE_VENDOR_VMWARE &&
           device->pci_header->common.device_id == VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2) {
            vmware_svga2_init(heap, device);
        } else if(device->pci_header->common.vendor_id == VIDEO_PCI_DEVICE_VENDOR_QEMU &&
                  device->pci_header->common.device_id == VIDEO_PCI_DEVICE_ID_QEMU_VGA) {
            video_qemu_vga_init(heap, device);
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "Unknown video device: %x:%x",
                     device->pci_header->common.vendor_id, device->pci_header->common.device_id);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

int8_t video_display_reinit(void) {
    pci_context_t* pci_context = pci_get_context();
    iterator_t* iter           = list_iterator_create(pci_context->display_controllers);

    if(iter == NULL) {
        return -1;
    }

    while(!iter->end_of_iterator(iter)) {
        const pci_dev_t* device = iter->get_item(iter);

        if(device->pci_header->common.vendor_id == VIDEO_PCI_DEVICE_VENDOR_VMWARE &&
           device->pci_header->common.device_id == VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2) {
            PRINTLOG(KERNEL, LOG_WARNING, "Not implemented reinit for vmware svga2");
        } else if(device->pci_header->common.vendor_id == VIDEO_PCI_DEVICE_VENDOR_QEMU &&
                  device->pci_header->common.device_id == VIDEO_PCI_DEVICE_ID_QEMU_VGA) {
            video_qemu_vga_reinit();
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "Unknown video device: %x:%x",
                     device->pci_header->common.vendor_id, device->pci_header->common.device_id);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}
