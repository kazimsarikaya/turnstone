/**
 * @file usb.64.c
 * @brief USB driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <driver/usb_ehci.h>
#include <driver/usb_xhci.h>
#include <pci.h>
#include <logging.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <hashmap.h>

MODULE("turnstone.kernel.hw.usb");


hashmap_t* usb_controllers = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t usb_init(void) {
    logging_set_level(USB, LOG_DEBUG);

    usb_controllers = hashmap_integer(64);

    if(!usb_controllers) {
        PRINTLOG(USB, LOG_ERROR, "cannot create hashmap");

        return -1;
    }


    iterator_t* it = list_iterator_create(pci_get_context()->usb_controllers);

    while(it->end_of_iterator(it) != 0) {
        usb_controller_t* usb_controller = memory_malloc(sizeof(usb_controller_t));

        if(!usb_controller) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb controller");

            return -1;
        }


        const pci_dev_t* pci_dev = it->get_item(it);
        pci_common_header_t* pci_header = pci_dev->pci_header;
        pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)pci_header;



        usb_controller->pci_dev = pci_dev;
        usb_controller->controller_id = hashmap_size(usb_controllers);
        hashmap_put(usb_controllers, (void*)usb_controller->controller_id, usb_controller);

        PRINTLOG(USB, LOG_TRACE, "USB controller found: %02x:%02x:%02x:%02x",
                 pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);

        PRINTLOG(USB, LOG_TRACE, "  Vendor ID: %04x", pci_header->vendor_id);
        PRINTLOG(USB, LOG_TRACE, "  Device ID: %04x", pci_header->device_id);
        PRINTLOG(USB, LOG_TRACE, "  Class: %02x", pci_header->class_code);
        PRINTLOG(USB, LOG_TRACE, "  Subclass: %02x", pci_header->subclass_code);
        PRINTLOG(USB, LOG_TRACE, "  Prog. IF: %02x", pci_header->prog_if);


        uint64_t bar_fa = pci_get_bar_address(pci_gen_dev, 0);

        PRINTLOG(USB, LOG_TRACE, "frame address at bar 0x%llx", bar_fa);

        frame_t* bar_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)bar_fa);
        uint64_t size = pci_get_bar_size(pci_gen_dev, 0);
        PRINTLOG(USB, LOG_TRACE, "bar size 0x%llx", size);
        uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        frame_t bar_req_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

        uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

        if(bar_frames == NULL) {
            PRINTLOG(USB, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);

            if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &bar_req_frm) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot allocate frame");

                return -1;
            }
        }

        memory_paging_add_va_for_frame(bar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        if(pci_gen_dev->common_header.status.capabilities_list) {
            pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_gen_dev) + pci_gen_dev->capabilities_pointer);

            while(pci_cap->capability_id != 0xFF) {
                PRINTLOG(USB, LOG_TRACE, "capability id: %i noff 0x%x", pci_cap->capability_id, pci_cap->next_pointer);


                if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                    PRINTLOG(USB, LOG_TRACE, "msix capability found");

                    pci_capability_msix_t* msix_cap = (pci_capability_msix_t*)pci_cap;


                    if(pci_msix_configure(pci_gen_dev, msix_cap) != 0) {
                        PRINTLOG(USB, LOG_ERROR, "failed to configure msix");

                        return NULL;
                    }

                    usb_controller->msix_cap = msix_cap;

                }

                if(pci_cap->next_pointer == NULL) {
                    break;
                }

                pci_cap = (pci_capability_t*)(((uint8_t*)pci_gen_dev) + pci_cap->next_pointer);
            }
        }

        if(pci_header->prog_if == USB_EHCI) {
            usb_controller->controller_type = USB_CONTROLLER_TYPE_EHCI;

            if(usb_ehci_init(usb_controller) != 0) {
                PRINTLOG(USB, LOG_ERROR, "failed to initialize EHCI");

                return -1;
            }
        } else if(pci_header->prog_if == USB_XHCI) {
            usb_controller->controller_type = USB_CONTROLLER_TYPE_XHCI;

            if(usb_xhci_init(usb_controller) != 0) {
                PRINTLOG(USB, LOG_ERROR, "failed to initialize XHCI");

                return -1;
            }
        }


        it = it->next(it);
    }


    it->destroy(it);


    return 0;
}
#pragma GCC diagnostic pop


int8_t usb_probe_all_devices_all_ports(void) {
    iterator_t* it = hashmap_iterator_create(usb_controllers);

    while(it->end_of_iterator(it) != 0) {
        usb_controller_t* usb_controller = (usb_controller_t*)it->get_item(it);

        if(usb_controller->initialized) {
            if(usb_controller->probe_all_ports(usb_controller) != 0) {
                PRINTLOG(USB, LOG_ERROR, "failed to probe all ports on %llx", usb_controller->controller_id);
            }
        }

        it = it->next(it);
    }

    it->destroy(it);

    return 0;
}
