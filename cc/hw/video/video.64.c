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
#include <logging.h>
#include <apic.h>
#include <ports.h>
#include <graphics/text_cursor.h>
#include <cpu/sync.h>

MODULE("turnstone.kernel.hw.video");

boolean_t GRAPHICS_MODE = false;

lock_t* video_lock = NULL;

video_graphics_print_f VIDEO_GRAPHICS_PRINT = NULL;

void video_print(const char_t* string) {
    lock_acquire(video_lock);

    video_text_print(string);

    if(GRAPHICS_MODE) {
        text_cursor_hide();
        VIDEO_GRAPHICS_PRINT(string);
        text_cursor_show();
    }

    lock_release(video_lock);
}

static const int32_t serial_ports[] = {0x3F8, 0x2F8, 0x3E8, 0x2E8};

void video_text_print(const char_t* string) {
    if(string == NULL) {
        return;
    }
    size_t i = 0;
    uint32_t apic_id = apic_get_local_apic_id();

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

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* device = iter->get_item(iter);

        if(device->pci_header->vendor_id == VIDEO_PCI_DEVICE_VENDOR_VMWARE && device->pci_header->device_id == VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2) {
            vmware_svga2_init(heap, device);
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "Unknown video device: %x:%x", device->pci_header->vendor_id, device->pci_header->device_id);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}
