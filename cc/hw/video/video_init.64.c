/**
 * @file video_init.64.c
 * @brief Video Initialization
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <list.h>
#include <pci.h>
#include <driver/video_virtio.h>
#include <driver/video_vmwaresvga.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.video.init");

int8_t video_display_init(memory_heap_t* heap, list_t* display_controllers) {
    iterator_t* iter = list_iterator_create(display_controllers);

    if(iter == NULL) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* device = iter->get_item(iter);

        if(device->pci_header->vendor_id == VIDEO_PCI_DEVICE_VENDOR_VIRTIO && device->pci_header->device_id == VIDEO_PCI_DEVICE_ID_VIRTIO_GPU) {
            virtio_video_init(heap, device);
        } else if(device->pci_header->vendor_id == VIDEO_PCI_DEVICE_VENDOR_VMWARE && device->pci_header->device_id == VIDEO_PCI_DEVICE_ID_VMWARE_SVGA2) {
            vmware_svga2_init(heap, device);
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "Unknown video device: %x:%x", device->pci_header->vendor_id, device->pci_header->device_id);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}
