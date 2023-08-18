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
#include <video.h>
#include <ports.h>
#include <cpu.h>
#include <cpu/interrupt.h>

MODULE("turnstone.kernel.hw.pci");

typedef struct {
    memory_heap_t*     heap;
    acpi_table_mcfg_t* mcfg;
    uint16_t           group_number_count;
    uint16_t           group_number;
    uint8_t            bus_number;
    uint8_t            device_number;
    uint8_t            function_number;
    int8_t             end_of_iter;
}pci_iterator_internal_t;

int8_t      pci_iterator_destroy(iterator_t* iterator);
iterator_t* pci_iterator_next(iterator_t* iterator);
int8_t      pci_iterator_end_of_iterator(iterator_t* iterator);
const void* pci_iterator_get_item(iterator_t* iterator);

pci_context_t* PCI_CONTEXT = NULL;

int8_t pci_msix_set_isr(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector, interrupt_irq isr) {
    uint64_t msix_table_address = pci_get_bar_address(pci_dev, msix_cap->bir);;
    msix_table_address += msix_cap->table_offset;

    pci_capability_msix_table_t* msix_table = (pci_capability_msix_table_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_table_address);

    uint8_t intnum = interrupt_get_next_empty_interrupt();
    msix_table->entries[msix_vector].message_address = 0xFEE00000;
    msix_table->entries[msix_vector].message_data = intnum;
    msix_table->entries[msix_vector].masked = 0;

    uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
    interrupt_irq_set_handler(isrnum, isr);

    PRINTLOG(PCI, LOG_TRACE, "msix table %p vector 0x%x isr 0x%02x", msix_table, msix_vector,  msix_table->entries[msix_vector].message_data);

    return 0;
}

int8_t pci_msix_clear_pending_bit(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector) {
    uint64_t msix_pendind_bit_table_address = pci_get_bar_address(pci_dev, msix_cap->pending_bit_bir);

    msix_pendind_bit_table_address += msix_cap->pending_bit_offset;

    uint8_t* pending_bit_table = (uint8_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(msix_pendind_bit_table_address);

    uint8_t mask1 = (1 << msix_cap->table_size) - 1;

    uint8_t mask2 = ~(1 << msix_vector);
    mask1 &=  mask2;

    *pending_bit_table &= mask1;

    PRINTLOG(PCI, LOG_TRACE, "pending bit cleared %i",  msix_vector);

    return 0;
}

uint64_t pci_get_bar_size(pci_generic_device_t* pci_dev, uint8_t bar_no){
    uint64_t old_address = pci_get_bar_address(pci_dev, bar_no);
    uint64_t mask = -1ULL;
    pci_set_bar_address(pci_dev, bar_no, mask);

    uint64_t size = pci_get_bar_address(pci_dev, bar_no);

    pci_set_bar_address(pci_dev, bar_no, old_address);

    size = ~size + 1;

    PRINTLOG(PCI, LOG_TRACE, "bar %i size 0x%llx",  bar_no, size);

    return size;
}

uint64_t pci_get_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no){
    pci_bar_register_t* bar = &pci_dev->bar0;
    bar += bar_no;

    uint64_t bar_fa = 0;

    if(bar->bar_type.type == 0) {
        bar_fa = (uint64_t)bar->memory_space_bar.base_address;
        bar_fa <<= 4;

        if(bar->memory_space_bar.type == 2) {
            bar++;
            uint64_t tmp = (uint64_t)(*((uint32_t*)bar));
            bar_fa = tmp << 32 | bar_fa;
        }

    }

    PRINTLOG(PCI, LOG_TRACE, "bar %i address 0x%llx",  bar_no, bar_fa);

    return bar_fa;
}

int8_t pci_set_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no, uint64_t bar_fa){
    pci_bar_register_t* bar = &pci_dev->bar0;
    bar += bar_no;

    if(bar->bar_type.type == 0) {
        bar->memory_space_bar.base_address = (bar_fa >> 4) & 0xFFFFFFF;

        if(bar->memory_space_bar.type == 2) {
            bar++;

            (*(uint64_t*)bar) = bar_fa >> 32;
        }

    } else {
        return -1;
    }

    return 0;
}

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
    PCI_CONTEXT->sata_controllers = linkedlist_create_list_with_heap(heap);
    PCI_CONTEXT->nvme_controllers = linkedlist_create_list_with_heap(heap);
    PCI_CONTEXT->network_controllers = linkedlist_create_list_with_heap(heap);
    PCI_CONTEXT->other_devices = linkedlist_create_list_with_heap(heap);

    linkedlist_t old_mcfgs = linkedlist_create_list();

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

            PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> %04x:%04x -> %02x:%02x",
                     p->group_number, p->bus_number, p->device_number, p->function_number,
                     p->pci_header->vendor_id, p->pci_header->device_id,
                     p->pci_header->class_code, p->pci_header->subclass_code);

            if( p->pci_header->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_SATA_CONTROLLER) {

                linkedlist_list_insert(PCI_CONTEXT->sata_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as sata controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( p->pci_header->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                       p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_NVME_CONTROLLER) {

                linkedlist_list_insert(PCI_CONTEXT->nvme_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as nvme controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( p->pci_header->class_code == PCI_DEVICE_CLASS_NETWORK_CONTROLLER &&
                       p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_ETHERNET) {

                linkedlist_list_insert(PCI_CONTEXT->network_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as network controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else {
                linkedlist_list_insert(PCI_CONTEXT->other_devices, p);
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

        linkedlist_list_insert(old_mcfgs, mcfg);

        mcfg = (acpi_table_mcfg_t*)acpi_get_next_table(ACPI_CONTEXT->xrsdp_desc, "MCFG", old_mcfgs);
    }

    linkedlist_destroy(old_mcfgs);

    PRINTLOG(PCI, LOG_INFO, "pci devices enumeration completed");
    PRINTLOG(PCI, LOG_INFO, "total pci sata controllers %lli nvme controllers %lli network controllers %lli other devices %lli",
             linkedlist_size(PCI_CONTEXT->sata_controllers),
             linkedlist_size(PCI_CONTEXT->nvme_controllers),
             linkedlist_size(PCI_CONTEXT->network_controllers),
             linkedlist_size(PCI_CONTEXT->other_devices)
             );

    return 0;
}
#pragma GCC diagnostic pop

int8_t pci_io_port_write_data(uint32_t address, uint32_t data, uint8_t bc) {
    outl(address, PCI_IO_PORT_CONFIG);

    switch (bc) {
    case 1:
        outb(PCI_IO_PORT_DATA, data & 0xFF);
        break;
    case 2:
        outw(PCI_IO_PORT_DATA, data & 0xFFFF);
        break;
    case 4:
        outl(PCI_IO_PORT_DATA, data);
        break;
    }

    return 0;
}

uint32_t pci_io_port_read_data(uint32_t address, uint8_t bc) {
    outl(address, PCI_IO_PORT_CONFIG);

    uint32_t res = 0;

    switch (bc) {
    case 1:
        res = inb(PCI_IO_PORT_DATA);
        break;
    case 2:
        res = inw(PCI_IO_PORT_DATA);
        break;
    case 4:
        res = inl(PCI_IO_PORT_DATA);
        break;
    }

    return res;
}


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

                    //calculate mmio address of device
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

                //calculate mmio address of device
                size_t pci_mmio_addr_fa = iter_metadata->mcfg->pci_segment_group_config[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | 0 << 12 );
                size_t pci_mmio_addr_va  = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_mmio_addr_fa);


                frame_t* pci_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)pci_mmio_addr_fa);

                if(pci_frames == NULL) {
                    PRINTLOG(PCI, LOG_ERROR, "cannot find frames of pci dev 0x%016llx", pci_mmio_addr_fa);
                    iter_metadata->end_of_iter = 0; //end iter

                    return iterator;
                } else if((pci_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                    PRINTLOG(PCI, LOG_DEBUG, "frames of pci dev 0x%016llx is 0x%llx 0x%llx", pci_mmio_addr_fa, pci_frames->frame_address, pci_frames->frame_count);
                    memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_frames->frame_address), pci_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                    pci_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                }

                pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr_va;

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
                                    iter_metadata->end_of_iter = 0; //end iter

                                    return iterator;
                                } else if((pci_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                                    PRINTLOG(PCI, LOG_DEBUG, "frames of pci dev 0x%016llx is 0x%llx 0x%llx", pci_mmio_addr_f_fa, pci_frames->frame_address, pci_frames->frame_count);
                                    memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_frames->frame_address), pci_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                                    pci_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                                }

                                pci_hdr = (pci_common_header_t*)pci_mmio_addr_f_va;

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

            } //end of device loop

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
    } //end of bus group loop

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
    d->pci_header = (pci_common_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA( (iter_metadata->mcfg->pci_segment_group_config[d->group_number].base_address + (((size_t)d->bus_number ) << 20 | ((size_t)d->device_number) << 15 |  ((size_t)d->function_number ) << 12) ) );
    return d;
}
