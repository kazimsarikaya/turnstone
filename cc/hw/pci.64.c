/**
 * @file pci.64.c
 * @brief pci implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <types.h>
#include <pci.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <acpi.h>
#include <utils.h>
#include <logging.h>
#include <ports.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <time/timer.h>

MODULE("turnstone.kernel.hw.pci");

typedef struct {
    memory_heap_t*     heap;
    acpi_table_mcfg_t* mcfg;
    uint16_t           group_number_count;
    uint16_t           group_number;
    uint8_t            bus_number;
    uint8_t            device_number;
    uint8_t            function_number;
    uint64_t           pci_mmio_addr_fa;
    uint64_t           pci_mmio_addr_va;
    uint64_t           frame_count;
    int8_t             end_of_iter;
}pci_iterator_internal_t;

int8_t      pci_iterator_destroy(iterator_t* iterator);
iterator_t* pci_iterator_next(iterator_t* iterator);
int8_t      pci_iterator_end_of_iterator(iterator_t* iterator);
const void* pci_iterator_get_item(iterator_t* iterator);

pci_context_t* PCI_CONTEXT = NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t pci_setup(memory_heap_t* heap) {

    acpi_table_mcfg_t* mcfg = ACPI_CONTEXT->mcfg;

    if(mcfg == NULL) {
        PRINTLOG(PCI, LOG_FATAL, "there is not mcfg table, pci enumeration isnot supported");

        return -1;
    }

    PRINTLOG(PCI, LOG_INFO, "pci devices enumerating");

    PCI_CONTEXT = memory_malloc_ext(heap, sizeof(pci_context_t), 0);
    PCI_CONTEXT->sata_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->nvme_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->network_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->display_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->usb_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->input_controllers = list_create_list_with_heap(heap);
    PCI_CONTEXT->other_devices = list_create_list_with_heap(heap);

    list_t* old_mcfgs = list_create_list();

    while(mcfg) {
        PRINTLOG(PCI, LOG_TRACE, "mcfg is found at 0x%p", mcfg);


        iterator_t* iter = pci_iterator_create_with_heap(heap, mcfg);

        if(iter == NULL) {
            return -1;
        }

        while(iter->end_of_iterator(iter) != 0) {
            const pci_dev_t* p = iter->get_item(iter);

            if(p == NULL) {
                iter->destroy(iter);

                return -1;
            }

            pci_disable_interrupt((pci_generic_device_t*)p->pci_header);

            PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> %04x:%04x -> %02x:%02x",
                     p->group_number, p->bus_number, p->device_number, p->function_number,
                     p->pci_header->vendor_id, p->pci_header->device_id,
                     p->pci_header->class_code, p->pci_header->subclass_code);

            if( p->pci_header->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_SATA_CONTROLLER) {

                list_list_insert(PCI_CONTEXT->sata_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as sata controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( p->pci_header->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                       p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_NVME_CONTROLLER) {

                list_list_insert(PCI_CONTEXT->nvme_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as nvme controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( p->pci_header->class_code == PCI_DEVICE_CLASS_NETWORK_CONTROLLER &&
                       p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_ETHERNET) {

                list_list_insert(PCI_CONTEXT->network_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as network controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if(p->pci_header->class_code == PCI_DEVICE_CLASS_DISPLAY_CONTROLLER) {

                list_list_insert(PCI_CONTEXT->display_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as display controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( p->pci_header->class_code == PCI_DEVICE_CLASS_SERIAL_BUS &&
                       p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_USB_CONTROLLER) {

                list_list_insert(PCI_CONTEXT->usb_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as usb controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            }  else if( p->pci_header->class_code == PCI_DEVICE_CLASS_INPUT_DEVICE) {

                list_list_insert(PCI_CONTEXT->input_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as usb controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            }else {
                PRINTLOG(PCI, LOG_WARNING, "pci dev %02x:%02x:%02x.%02x class %02x:%02x is not supported",
                         p->group_number, p->bus_number, p->device_number, p->function_number, p->pci_header->class_code, p->pci_header->subclass_code);

                list_list_insert(PCI_CONTEXT->other_devices, p);

                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as other device",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            }


            if(p->pci_header->header_type.header_type == PCI_HEADER_TYPE_GENERIC_DEVICE) {
                pci_generic_device_t* pg = (pci_generic_device_t*)p->pci_header;

                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> pif %02x int %02x:%02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         pg->common_header.prog_if, pg->interrupt_line, pg->interrupt_pin);

                if(pg->common_header.status.capabilities_list) {
                    pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pg) + pg->capabilities_pointer);


                    while(pci_cap->capability_id != 0xFF) {
                        PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> cap 0x%x",
                                 p->group_number, p->bus_number, p->device_number, p->function_number, pci_cap->capability_id);

                        if(pci_cap->next_pointer == NULL) {
                            break;
                        }

                        pci_cap = (pci_capability_t*)(((uint8_t*)pci_cap ) + pci_cap->next_pointer);
                    }
                }
            }

            iter = iter->next(iter);
        }
        iter->destroy(iter);

        list_list_insert(old_mcfgs, mcfg);

        mcfg = (acpi_table_mcfg_t*)acpi_get_next_table(ACPI_CONTEXT->xrsdp_desc, "MCFG", old_mcfgs);
    }

    list_destroy(old_mcfgs);

    PRINTLOG(PCI, LOG_INFO, "pci devices enumeration completed");
    PRINTLOG(PCI, LOG_INFO, "total pci sata controllers %lli nvme controllers %lli network controllers %lli display controllers %lli usb controllers %lli input controllers %lli other devices %lli",
             list_size(PCI_CONTEXT->sata_controllers),
             list_size(PCI_CONTEXT->nvme_controllers),
             list_size(PCI_CONTEXT->network_controllers),
             list_size(PCI_CONTEXT->display_controllers),
             list_size(PCI_CONTEXT->usb_controllers),
             list_size(PCI_CONTEXT->input_controllers),
             list_size(PCI_CONTEXT->other_devices)
             );

    return 0;
}
#pragma GCC diagnostic pop


iterator_t* pci_iterator_create_with_heap(memory_heap_t* heap, acpi_table_mcfg_t* mcfg){
    iterator_t* iter = memory_malloc_ext(heap, sizeof(iterator_t), 0x0);

    if(iter == NULL) {
        return NULL;
    }

    pci_iterator_internal_t* iter_metadata = memory_malloc_ext(heap, sizeof(pci_iterator_internal_t), 0x0);

    if(iter_metadata == NULL) {
        memory_free_ext(heap, iter);

        return NULL;
    }

    iter_metadata->heap = heap;
    iter_metadata->mcfg = mcfg;

    size_t count = mcfg->header.length - sizeof(acpi_sdt_header_t) - sizeof_field(acpi_table_mcfg_t, reserved0);
    count /= 16;
    iter_metadata->group_number_count = count;

    int8_t dev_found = -1;

    for(size_t i = 0; i < iter_metadata->group_number_count; i++) {
        iter_metadata->group_number = i;
        for(size_t bus_addr = mcfg->pci_segment_group_config[i].bus_start;
            bus_addr <= mcfg->pci_segment_group_config[i].bus_end;
            bus_addr++) {
            iter_metadata->bus_number = bus_addr;
            for(size_t dev_addr = 0; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
                iter_metadata->device_number = dev_addr;
                for(size_t func_addr = 0; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
                    iter_metadata->function_number = func_addr;

                    // calculate mmio address of device
                    size_t pci_mmio_addr_fa = iter_metadata->mcfg->pci_segment_group_config[i].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
                    size_t pci_mmio_addr_va  = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_mmio_addr_fa);


                    frame_t* pci_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)pci_mmio_addr_fa);

                    if(pci_frames == NULL) {
                        PRINTLOG(PCI, LOG_ERROR, "cannot find frames of mmio 0x%016llx", pci_mmio_addr_fa);
                        memory_free_ext(heap, iter_metadata);
                        memory_free_ext(heap, iter);

                        return NULL;
                    } else if((pci_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                        PRINTLOG(PCI, LOG_DEBUG, "frames of mmio 0x%016llx is 0x%llx 0x%llx", pci_mmio_addr_fa, pci_frames->frame_address, pci_frames->frame_count);
                        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_frames->frame_address), pci_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                        pci_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                    }

                    pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr_va;

                    iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                    iter_metadata->pci_mmio_addr_va = pci_mmio_addr_va;
                    iter_metadata->frame_count = pci_frames->frame_count;

                    if(pci_hdr->vendor_id != 0xFFFF) {
                        dev_found = 0;
                        break;
                    }

                } // func_addr loop
                if(dev_found == 0) {
                    break;
                }
            } // dev_addr loop
            if(dev_found == 0) {
                break;
            }
        } // bus_addr loop
        if(dev_found == 0) {
            break;
        }
    } // bus_group loop

    if(dev_found == 0) {
        iter_metadata->end_of_iter = -1;
    } else {
        iter_metadata->end_of_iter = 0;
    }

    iter->metadata = iter_metadata;
    iter->destroy = &pci_iterator_destroy;
    iter->next = &pci_iterator_next;
    iter->end_of_iterator = &pci_iterator_end_of_iterator;
    iter->get_item = &pci_iterator_get_item;
    return iter;
}


int8_t pci_iterator_destroy(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    memory_heap_t* heap = iter_metadata->heap;
    memory_free_ext(heap, iter_metadata);
    memory_free_ext(heap, iterator);
    return 0;
}

iterator_t* pci_iterator_next(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;

    int8_t dev_found = -1;
    int8_t check_func0 = -1;

    for(size_t bus_group = iter_metadata->group_number; bus_group < iter_metadata->group_number_count; bus_group++) {
        iter_metadata->group_number = bus_group;

        for(size_t bus_addr = iter_metadata->bus_number;
            bus_addr < iter_metadata->mcfg->pci_segment_group_config[bus_group].bus_end;
            bus_addr++) {
            iter_metadata->bus_number = bus_addr;

            for(size_t dev_addr = iter_metadata->device_number; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
                iter_metadata->device_number = dev_addr;

                // calculate mmio address of device
                size_t pci_mmio_addr_fa = iter_metadata->mcfg->pci_segment_group_config[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | 0 << 12 );
                size_t pci_mmio_addr_va  = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_mmio_addr_fa);


                frame_t* pci_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)pci_mmio_addr_fa);

                if(pci_frames == NULL) {
                    PRINTLOG(PCI, LOG_ERROR, "cannot find frames of pci dev 0x%016llx", pci_mmio_addr_fa);
                    iter_metadata->end_of_iter = 0; // end iter

                    return iterator;
                } else if((pci_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                    PRINTLOG(PCI, LOG_DEBUG, "frames of pci dev 0x%016llx is 0x%llx 0x%llx", pci_mmio_addr_fa, pci_frames->frame_address, pci_frames->frame_count);
                    memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_frames->frame_address), pci_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                    pci_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                }

                pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr_va;

                iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                iter_metadata->pci_mmio_addr_va = pci_mmio_addr_va;
                iter_metadata->frame_count = pci_frames->frame_count;

                if(pci_hdr->vendor_id != 0xFFFF) { // look for vendor_id
                    if(check_func0 == 0) { // one/multi func check?
                        dev_found = 0;

                        break;
                    } else {
                        if(pci_hdr->header_type.multifunction == 1) {
                            iter_metadata->function_number++;

                            for(size_t func_addr = iter_metadata->function_number; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
                                iter_metadata->function_number = func_addr;

                                size_t pci_mmio_addr_f_fa = iter_metadata->mcfg->pci_segment_group_config[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
                                size_t pci_mmio_addr_f_va  = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_mmio_addr_f_fa);

                                pci_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)pci_mmio_addr_f_fa);

                                if(pci_frames == NULL) {
                                    PRINTLOG(PCI, LOG_ERROR, "cannot find frames of pci dev 0x%016llx", pci_mmio_addr_fa);
                                    iter_metadata->end_of_iter = 0; // end iter

                                    return iterator;
                                } else if((pci_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                                    PRINTLOG(PCI, LOG_DEBUG, "frames of pci dev 0x%016llx is 0x%llx 0x%llx", pci_mmio_addr_f_fa, pci_frames->frame_address, pci_frames->frame_count);
                                    memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_frames->frame_address), pci_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                                    pci_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                                }

                                pci_hdr = (pci_common_header_t*)pci_mmio_addr_f_va;

                                iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                                iter_metadata->pci_mmio_addr_va = pci_mmio_addr_f_va;
                                iter_metadata->frame_count = pci_frames->frame_count;

                                if(pci_hdr->vendor_id != 0xFFFF) {
                                    dev_found = 0;

                                    break;
                                }
                            }
                        } // end of one/multi check

                    }
                }

                if(dev_found == 0) {
                    break;
                } else {
                    iter_metadata->function_number = 0;
                    check_func0 = 0;
                }

            } // end of device loop

            if(dev_found == 0) {
                break;
            } else {
                iter_metadata->device_number = 0;
            }
        } // bus loop end

        if(dev_found == 0) {
            break;
        }else if((bus_group + 1) < iter_metadata->group_number_count) {
            iter_metadata->bus_number = iter_metadata->mcfg->pci_segment_group_config[bus_group + 1].bus_start;
        }
    } // end of bus group loop

    if(dev_found == -1) {
        iter_metadata->end_of_iter = 0;
    }

    return iterator;
}

int8_t pci_iterator_end_of_iterator(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    return iter_metadata->end_of_iter;
}

const void* pci_iterator_get_item(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    memory_heap_t* heap = iter_metadata->heap;
    pci_dev_t* d = memory_malloc_ext(heap, sizeof(pci_dev_t), 0x0);

    if(d == NULL) {
        return NULL;
    }

    d->group_number = iter_metadata->group_number;
    d->bus_number = iter_metadata->bus_number;
    d->device_number = iter_metadata->device_number;
    d->function_number = iter_metadata->function_number;
    d->pci_header = (pci_common_header_t*)iter_metadata->pci_mmio_addr_va;
    d->header_size = iter_metadata->frame_count * FRAME_SIZE;

    return d;
}
