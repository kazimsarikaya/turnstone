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
#include <acpi/aml.h>
#include <utils.h>
#include <logging.h>
#include <ports.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <time/timer.h>
#include <device/mmio.h>

MODULE("turnstone.kernel.hw.pci");

typedef struct pci_iterator_internal_t {
    memory_heap_t*     heap;
    acpi_table_mcfg_t* mcfg;
    uint16_t           group_number_count;
    uint16_t           group_number;
    uint8_t            bus_number;
    int32_t            old_bus_number;
    pci_dev_t*         bus_parent;
    uint8_t            device_number;
    int32_t            old_device_number;
    pci_dev_t*         device_parent;
    uint8_t            function_number;
    uint64_t           pci_mmio_addr_fa;
    uint64_t           pci_mmio_addr_va;
    int8_t             end_of_iter;
}pci_iterator_internal_t;

static int8_t pci_iterator_destroy(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    memory_heap_t* heap                    = iter_metadata->heap;
    memory_free_ext(heap, iter_metadata);
    memory_free_ext(heap, iterator);
    return 0;
}

static iterator_t* pci_iterator_next(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;

    boolean_t dev_found   = false;
    boolean_t check_func0 = false;

    for(size_t bus_group = iter_metadata->group_number; bus_group < iter_metadata->group_number_count; bus_group++) {
        iter_metadata->group_number = bus_group;

        for(size_t bus_addr = iter_metadata->bus_number;
            bus_addr < iter_metadata->mcfg->pci_segment_group_configs[bus_group].bus_end;
            bus_addr++) {
            iter_metadata->bus_number = bus_addr;

            for(size_t dev_addr = iter_metadata->device_number; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
                iter_metadata->device_number = dev_addr;

                // calculate mmio address of device
                size_t pci_mmio_addr_fa = iter_metadata->mcfg->pci_segment_group_configs[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | 0 << 12 );
                size_t pci_mmio_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, pci_mmio_addr_fa);

                pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr_va;

                iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                iter_metadata->pci_mmio_addr_va = pci_mmio_addr_va;

                if(pci_hdr->vendor_id != 0xFFFF) { // look for vendor_id
                    if(check_func0) { // one/multi func check?
                        dev_found = true;

                        break;
                    } else {
                        if(pci_hdr->header_type.multifunction == 1) {
                            iter_metadata->function_number++;

                            for(size_t func_addr = iter_metadata->function_number; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
                                iter_metadata->function_number = func_addr;

                                size_t pci_mmio_addr_f_fa = iter_metadata->mcfg->pci_segment_group_configs[bus_group].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
                                size_t pci_mmio_addr_f_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, pci_mmio_addr_f_fa);

                                pci_hdr = (pci_common_header_t*)pci_mmio_addr_f_va;

                                iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                                iter_metadata->pci_mmio_addr_va = pci_mmio_addr_f_va;

                                if(pci_hdr->vendor_id != 0xFFFF) {
                                    dev_found = true;

                                    break;
                                }
                            }
                        } // end of one/multi check

                    }
                }

                if(dev_found) {
                    break;
                } else {
                    iter_metadata->function_number = 0;
                    check_func0                    = true;
                }

            } // end of device loop

            if(dev_found) {
                break;
            } else {
                iter_metadata->device_number = 0;
            }
        } // bus loop end

        if(dev_found) {
            break;
        }else if((bus_group + 1) < iter_metadata->group_number_count) {
            iter_metadata->bus_number = iter_metadata->mcfg->pci_segment_group_configs[bus_group + 1].bus_start;
        }
    } // end of bus group loop

    if(!dev_found) {
        iter_metadata->end_of_iter = true;
    }

    return iterator;
}

static boolean_t pci_iterator_end_of_iterator(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    return iter_metadata->end_of_iter;
}

static const void* pci_iterator_get_item(iterator_t* iterator){
    pci_iterator_internal_t* iter_metadata = (pci_iterator_internal_t*)iterator->metadata;
    memory_heap_t* heap                    = iter_metadata->heap;
    pci_dev_t* d                           = memory_malloc_ext(heap, sizeof(pci_dev_t), 0x0);

    if(d == NULL) {
        return NULL;
    }

    d->group_number    = iter_metadata->group_number;
    d->bus_number      = iter_metadata->bus_number;
    d->device_number   = iter_metadata->device_number;
    d->function_number = iter_metadata->function_number;
    d->pci_header      = (pci_device_header_t*)iter_metadata->pci_mmio_addr_va;

    if(iter_metadata->old_bus_number != d->bus_number) {
        iter_metadata->old_bus_number    = d->bus_number;
        iter_metadata->bus_parent        = d;
        iter_metadata->old_device_number = -1; // reset device number for new bus
        iter_metadata->device_parent     = NULL; // reset device parent for new bus
        d->parent                        = NULL;
    } else {
        d->parent = iter_metadata->bus_parent;
    }

    if(iter_metadata->old_device_number != d->device_number) {
        iter_metadata->old_device_number = d->device_number;
        iter_metadata->device_parent     = d;
    } else {
        if(d->function_number == 0) {
            d->parent = iter_metadata->bus_parent;
        } else {
            d->parent = iter_metadata->device_parent;
        }
    }

    return d;
}

static iterator_t* pci_iterator_create_with_heap(memory_heap_t* heap, acpi_table_mcfg_t* mcfg){
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

    iter_metadata->group_number_count = ACPI_MCFG_PCI_SEGMENT_GROUP_CONFIG_COUNT(mcfg);

    iter_metadata->old_bus_number    = -1;
    iter_metadata->old_device_number = -1;

    boolean_t dev_found = false;

    for(size_t i = 0; i < iter_metadata->group_number_count; i++) {
        iter_metadata->group_number = i;
        for(size_t bus_addr = mcfg->pci_segment_group_configs[i].bus_start;
            bus_addr <= mcfg->pci_segment_group_configs[i].bus_end;
            bus_addr++) {
            iter_metadata->bus_number = bus_addr;
            for(size_t dev_addr = 0; dev_addr < PCI_DEVICE_MAX_COUNT; dev_addr++) {
                iter_metadata->device_number = dev_addr;
                for(size_t func_addr = 0; func_addr < PCI_FUNCTION_MAX_COUNT; func_addr++) {
                    iter_metadata->function_number = func_addr;

                    // calculate mmio address of device
                    size_t pci_mmio_addr_fa = iter_metadata->mcfg->pci_segment_group_configs[i].base_address + ( bus_addr << 20 | dev_addr << 15 | func_addr << 12 );
                    size_t pci_mmio_addr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, pci_mmio_addr_fa);

                    pci_common_header_t* pci_hdr = (pci_common_header_t*)pci_mmio_addr_va;

                    iter_metadata->pci_mmio_addr_fa = pci_mmio_addr_fa;
                    iter_metadata->pci_mmio_addr_va = pci_mmio_addr_va;

                    if(pci_hdr->vendor_id != 0xFFFF) {
                        dev_found = true;
                        break;
                    }

                } // func_addr loop
                if(dev_found) {
                    break;
                }
            } // dev_addr loop
            if(dev_found) {
                break;
            }
        } // bus_addr loop
        if(dev_found) {
            break;
        }
    } // bus_group loop

    if(dev_found) {
        iter_metadata->end_of_iter = false;
    } else {
        iter_metadata->end_of_iter = true;
    }

    iter->metadata        = iter_metadata;
    iter->destroy         = &pci_iterator_destroy;
    iter->next            = &pci_iterator_next;
    iter->end_of_iterator = &pci_iterator_end_of_iterator;
    iter->get_item        = &pci_iterator_get_item;
    return iter;
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

    pci_context_t* pci_context = memory_malloc_ext(heap, sizeof(pci_context_t), 0);

    if(pci_context == NULL) {
        return -1;
    }

    pci_context->heap                = heap;
    pci_context->all_devices         = list_create_list_with_heap(heap);
    pci_context->bridge_controllers  = list_create_list_with_heap(heap);
    pci_context->sata_controllers    = list_create_list_with_heap(heap);
    pci_context->nvme_controllers    = list_create_list_with_heap(heap);
    pci_context->network_controllers = list_create_list_with_heap(heap);
    pci_context->display_controllers = list_create_list_with_heap(heap);
    pci_context->usb_controllers     = list_create_list_with_heap(heap);
    pci_context->input_controllers   = list_create_list_with_heap(heap);
    pci_context->other_devices       = list_create_list_with_heap(heap);

    pci_set_context(pci_context);

    list_t* old_mcfgs = list_create_list();

    while(mcfg) {
        PRINTLOG(PCI, LOG_TRACE, "mcfg is found at 0x%p", mcfg);


        iterator_t* iter = pci_iterator_create_with_heap(heap, mcfg);

        if(iter == NULL) {
            return -1;
        }

        while(!iter->end_of_iterator(iter)) {
            const pci_dev_t* p = iter->get_item(iter);

            if(p == NULL) {
                iter->destroy(iter);

                return -1;
            }

            if(acpi_device_associate_pci_dev_links((pci_dev_t*)p) != 0) {
                PRINTLOG(PCI, LOG_ERROR, "cannot associate pci device with acpi aml device");

                iter->destroy(iter);

                return -1;
            }

            pci_common_header_t* pchdr = &p->pci_header->common;


            PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x -> %04x:%04x -> %02x:%02x",
                     p->group_number, p->bus_number, p->device_number, p->function_number,
                     pchdr->vendor_id, pchdr->device_id,
                     pchdr->class_code, pchdr->subclass_code);

            if(p->parent) {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x parent is %02x:%02x:%02x.%02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         p->parent->group_number, p->parent->bus_number, p->parent->device_number, p->parent->function_number);
            } else {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x has no parent",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            }

            if(p->aml_device) {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x has aml device %s",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         p->aml_device->name);
                LOGBLOCK(PCI, LOG_TRACE) {
                    acpi_device_print(ACPI_CONTEXT->acpi_parser_context, p->aml_device);
                }
            } else {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x has no aml device",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            }

            if(p->pci_root_bridge_aml_device) {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x has pci root bridge aml device %s",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         p->pci_root_bridge_aml_device->name);
                LOGBLOCK(PCI, LOG_TRACE) {
                    acpi_device_print(ACPI_CONTEXT->acpi_parser_context, p->pci_root_bridge_aml_device);
                }
            } else {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x has no pci root bridge aml device",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            }

            uintptr_t pci_hdr_addr = (uintptr_t)p->pci_header;


            for(size_t i = 0; i < ARRAY_SIZE(p->header_data.u32_data); i++) {
                uint32_t value = mmio_read(pci_hdr_addr + i * sizeof(uint32_t), sizeof(uint32_t));
                ((pci_dev_t*)p)->header_data.u32_data[i] = value;
            }

            list_queue_push(pci_context->all_devices, p);

            if(pchdr->class_code == PCI_DEVICE_CLASS_BRIDGE_CONTROLLER &&
               pchdr->subclass_code == PCI_DEVICE_SUBCLASS_BRIDGE_ISA) {

                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x is isa bridge",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            }

            if(pchdr->class_code == PCI_DEVICE_CLASS_BRIDGE_CONTROLLER &&
               pchdr->subclass_code == PCI_DEVICE_SUBCLASS_BRIDGE_PCI) {

                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x is pci bridge",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

                pci_pci2pci_bridge_t* bridge = (pci_pci2pci_bridge_t*)pchdr;

                PRINTLOG(PCI, LOG_TRACE, "pci bridge %02x:%02x:%02x.%02x -> primary bus %02x secondary bus %02x subordinate bus %02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         bridge->primary_bus_number, bridge->secondary_bus_number, bridge->subordinate_bus_number);

            }

            if( pchdr->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                pchdr->subclass_code == PCI_DEVICE_SUBCLASS_SATA_CONTROLLER) {

                list_list_insert(pci_context->sata_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as sata controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( pchdr->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
                       pchdr->subclass_code == PCI_DEVICE_SUBCLASS_NVME_CONTROLLER) {

                list_list_insert(pci_context->nvme_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as nvme controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( pchdr->class_code == PCI_DEVICE_CLASS_NETWORK_CONTROLLER &&
                       pchdr->subclass_code == PCI_DEVICE_SUBCLASS_ETHERNET) {

                list_list_insert(pci_context->network_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as network controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if(pchdr->class_code == PCI_DEVICE_CLASS_DISPLAY_CONTROLLER) {

                list_list_insert(pci_context->display_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as display controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( pchdr->class_code == PCI_DEVICE_CLASS_SERIAL_BUS &&
                       pchdr->subclass_code == PCI_DEVICE_SUBCLASS_USB_CONTROLLER) {

                list_list_insert(pci_context->usb_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as usb controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( pchdr->class_code == PCI_DEVICE_CLASS_INPUT_DEVICE) {

                list_list_insert(pci_context->input_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as input controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);

            } else if( pchdr->class_code == PCI_DEVICE_CLASS_BRIDGE_CONTROLLER) {

                list_list_insert(pci_context->bridge_controllers, p);
                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as bridge controller",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            } else {
                PRINTLOG(PCI, LOG_WARNING, "pci dev %02x:%02x:%02x.%02x class %02x:%02x (%02x) has no specific handler. Inserting as other device",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         pchdr->class_code, pchdr->subclass_code, pchdr->prog_if);

                list_list_insert(pci_context->other_devices, p);

                PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x inserted as other device",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            }

            if(pchdr->header_type.header_type == PCI_HEADER_TYPE_GENERIC_DEVICE) {
                pci_generic_device_t* pg = (pci_generic_device_t*)pchdr;

                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> pif %02x int %02x:%02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         pg->common_header.prog_if, pg->interrupt_line, pg->interrupt_pin);

                if(pg->common_header.status.capabilities_list) {
                    pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pg) + pg->capabilities_pointer);

                    PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> cap pointer 0x%x",
                             p->group_number, p->bus_number, p->device_number, p->function_number,
                             pg->capabilities_pointer);


                    while(pci_cap->capability_id != 0xFF) {
                        PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x -> cap 0x%x next 0x%x",
                                 p->group_number, p->bus_number, p->device_number, p->function_number,
                                 pci_cap->capability_id, pci_cap->next_pointer);

                        if(pci_cap->next_pointer == NULL) {
                            break;
                        }

                        pci_cap = (pci_capability_t*)(((uint8_t*)pg ) + pci_cap->next_pointer);
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

    // find missing parents for devices
    {
        iterator_t* all_devs_iter = list_iterator_create(pci_context->all_devices);

        while(!all_devs_iter->end_of_iterator(all_devs_iter)) {
            const pci_dev_t* p = (pci_dev_t*)all_devs_iter->get_item(all_devs_iter);

            if(p->parent) {
                all_devs_iter = all_devs_iter->next(all_devs_iter);
                continue;
            }

            if(p->aml_device && p->pci_root_bridge_aml_device && p->aml_device == p->pci_root_bridge_aml_device) {
                all_devs_iter = all_devs_iter->next(all_devs_iter);
                continue;
            }

            if(p->bus_number == 0 && p->device_number == 0 && p->function_number == 0) {
                all_devs_iter = all_devs_iter->next(all_devs_iter);
                continue;
            }

            iterator_t* bridge_iter = list_iterator_create(pci_context->bridge_controllers);

            while(!bridge_iter->end_of_iterator(bridge_iter)) {
                const pci_dev_t* bridge = (pci_dev_t*)bridge_iter->get_item(bridge_iter);

                if(bridge->aml_device && bridge->pci_root_bridge_aml_device
                   && bridge->aml_device != bridge->pci_root_bridge_aml_device) {
                    bridge_iter = bridge_iter->next(bridge_iter);
                    continue;
                }

                if(bridge->bus_number > p->bus_number) {
                    bridge_iter = bridge_iter->next(bridge_iter);
                    continue;
                }

                pci_pci2pci_bridge_t* bridge_hdr = (pci_pci2pci_bridge_t*)bridge->pci_header;

                if(p->bus_number >= bridge_hdr->secondary_bus_number && p->bus_number <= bridge_hdr->subordinate_bus_number) {
                    ((pci_dev_t*)p)->parent = (pci_dev_t*)bridge;
                    break;
                }

                bridge_iter = bridge_iter->next(bridge_iter);
            }

            bridge_iter->destroy(bridge_iter);

            if(!p->parent) {
                PRINTLOG(PCI, LOG_WARNING, "pci dev %02x:%02x:%02x.%02x has no parent and is not pci root bridge. This device might be inaccessible",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
            } else {
                PRINTLOG(PCI, LOG_TRACE, "pci dev %02x:%02x:%02x.%02x parent is %02x:%02x:%02x.%02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number,
                         p->parent->group_number, p->parent->bus_number, p->parent->device_number, p->parent->function_number);
            }

            all_devs_iter = all_devs_iter->next(all_devs_iter);
        }

        all_devs_iter->destroy(all_devs_iter);
    }

    // set proximity domain for devices
    {
        iterator_t* all_devs_iter = list_iterator_create(pci_context->all_devices);

        while(!all_devs_iter->end_of_iterator(all_devs_iter)) {
            const pci_dev_t* p = (pci_dev_t*)all_devs_iter->get_item(all_devs_iter);

            if(acpi_device_set_proximity_domain((pci_dev_t*)p) != 0) {
                PRINTLOG(PCI, LOG_ERROR, "cannot set proximity domain for pci dev %02x:%02x:%02x.%02x",
                         p->group_number, p->bus_number, p->device_number, p->function_number);
                all_devs_iter->destroy(all_devs_iter);
                return -1;
            }

            PRINTLOG(PCI, LOG_DEBUG, "pci dev %02x:%02x:%02x.%02x proximity domain is %u",
                     p->group_number, p->bus_number, p->device_number, p->function_number,
                     p->proximity_domain);

            all_devs_iter = all_devs_iter->next(all_devs_iter);
        }

        all_devs_iter->destroy(all_devs_iter);
    }

    PRINTLOG(PCI, LOG_INFO, "pci devices enumeration completed");
    PRINTLOG(PCI, LOG_INFO, "total pci devices found %lli", list_size(pci_context->all_devices));
    PRINTLOG(PCI, LOG_INFO, "total pci bridge controllers %lli", list_size(pci_context->bridge_controllers));
    PRINTLOG(PCI, LOG_INFO, "total sata controllers %lli", list_size(pci_context->sata_controllers));
    PRINTLOG(PCI, LOG_INFO, "total nvme controllers %lli", list_size(pci_context->nvme_controllers));
    PRINTLOG(PCI, LOG_INFO, "total network controllers %lli", list_size(pci_context->network_controllers));
    PRINTLOG(PCI, LOG_INFO, "total display controllers %lli", list_size(pci_context->display_controllers));
    PRINTLOG(PCI, LOG_INFO, "total usb controllers %lli", list_size(pci_context->usb_controllers));
    PRINTLOG(PCI, LOG_INFO, "total input controllers %lli", list_size(pci_context->input_controllers));
    PRINTLOG(PCI, LOG_INFO, "total other devices %lli", list_size(pci_context->other_devices));

    return 0;
}
#pragma GCC diagnostic pop

int8_t pci_recollect_header_data(void) {
    pci_context_t* pci_context = pci_get_context();

    if(pci_context == NULL) {
        return -1;
    }

    list_t* all_devs = pci_context->all_devices;

    for(size_t i = 0; i < list_size(all_devs); i++) {
        const pci_dev_t* p = (pci_dev_t*)list_get_data_at_position(all_devs, i);

        uintptr_t pci_hdr_addr = (uintptr_t)p->pci_header;

        for(size_t j = 0; j < ARRAY_SIZE(p->header_data.u32_data); j++) {
            uint32_t value = mmio_read(pci_hdr_addr + j * sizeof(uint32_t), sizeof(uint32_t));
            ((pci_dev_t*)p)->header_data.u32_data[j] = value;
        }
    }

    return 0;
}

int8_t pci_restore_registers(void) {
    pci_context_t* pci_context = pci_get_context();

    if(pci_context == NULL) {
        return -1;
    }

    list_t* all_devs = pci_context->all_devices;

    for(size_t i = 0; i < list_size(all_devs); i++) {
        const pci_dev_t* p = (pci_dev_t*)list_get_data_at_position(all_devs, i);

        const pci_common_header_t* pchdr = &p->header_data.common;
        uintptr_t pci_hdr_addr           = (uintptr_t)p->pci_header;

        if(!pci_hdr_addr) {
            PRINTLOG(PCI, LOG_ERROR, "pci dev %02x:%02x:%02x.%02x has invalid pci header address 0x%p, skipping register restore",
                     p->group_number, p->bus_number, p->device_number, p->function_number, (void*)pci_hdr_addr);
            continue;
        }

        PRINTLOG(PCI, LOG_INFO, "restoring pci dev %02x:%02x:%02x.%02x registers",
                 p->group_number, p->bus_number, p->device_number, p->function_number);

        uintptr_t command_reg_addr = pci_hdr_addr + offsetof_field(pci_common_header_t, command);
        mmio_write(command_reg_addr, 0, sizeof(uint16_t));

        if(pchdr->header_type.header_type == PCI_HEADER_TYPE_GENERIC_DEVICE) {
            const uint32_t* u32_data = p->header_data.u32_data;
            uintptr_t bar_offset     = offsetof_field(pci_generic_device_t, bar0);
            size_t bar_u32_idx       = bar_offset / sizeof(uint32_t);


            uintptr_t bar_offset_addr = pci_hdr_addr + bar_offset;

            for(size_t j = 0; j < 6; j++) {
                uint32_t bar_value = u32_data[bar_u32_idx + j];
                mmio_write(bar_offset_addr + j * sizeof(uint32_t), bar_value, sizeof(uint32_t));
            }

            uintptr_t expension_rom_bar_offset      = offsetof_field(pci_generic_device_t, expension_rom_base_address);
            size_t expension_rom_bar_u32_idx        = expension_rom_bar_offset / sizeof(uint32_t);
            uintptr_t expension_rom_bar_offset_addr = pci_hdr_addr + expension_rom_bar_offset;

            uint32_t expension_rom_bar_value = u32_data[expension_rom_bar_u32_idx];
            mmio_write(expension_rom_bar_offset_addr, expension_rom_bar_value, sizeof(uint32_t));

            pci_generic_device_t* pg = (pci_generic_device_t*)pchdr;

            uintptr_t interrupt_line_reg_addr = pci_hdr_addr + offsetof_field(pci_generic_device_t, interrupt_line);
            mmio_write(interrupt_line_reg_addr, pg->interrupt_line, sizeof(uint8_t));
        }

        if(pchdr->header_type.header_type == PCI_HEADER_TYPE_PCI2PCI_BRIDGE) {
            const uint32_t* u32_data = p->header_data.u32_data;
            uintptr_t bar_offset     = offsetof_field(pci_generic_device_t, bar0);
            size_t bar_u32_idx       = bar_offset / sizeof(uint32_t);


            uintptr_t bar_offset_addr = pci_hdr_addr + bar_offset;

            for(size_t j = 0; j < 2; j++) {
                uint32_t bar_value = u32_data[bar_u32_idx + j];
                mmio_write(bar_offset_addr + j * sizeof(uint32_t), bar_value, sizeof(uint32_t));
            }

            const pci_pci2pci_bridge_t* bridge = (const pci_pci2pci_bridge_t*)pchdr;

            uintptr_t primary_bus_reg_addr     = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, primary_bus_number);
            uintptr_t secondary_bus_reg_addr   = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, secondary_bus_number);
            uintptr_t subordinate_bus_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, subordinate_bus_number);

            mmio_write(primary_bus_reg_addr, bridge->primary_bus_number, sizeof(uint8_t));
            mmio_write(secondary_bus_reg_addr, bridge->secondary_bus_number, sizeof(uint8_t));
            mmio_write(subordinate_bus_reg_addr, bridge->subordinate_bus_number, sizeof(uint8_t));

            uintptr_t io_base_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, io_base);
            mmio_write(io_base_reg_addr, bridge->io_base, sizeof(uint8_t));

            uintptr_t io_limit_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, io_limit);
            mmio_write(io_limit_reg_addr, bridge->io_limit, sizeof(uint8_t));

            uintptr_t io_base_upper_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, io_base_upper_16bits);
            mmio_write(io_base_upper_reg_addr, bridge->io_base_upper_16bits, sizeof(uint16_t));

            uintptr_t io_limit_upper_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, io_limit_upper_16bits);
            mmio_write(io_limit_upper_reg_addr, bridge->io_limit_upper_16bits, sizeof(uint16_t));

            uintptr_t memory_base_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, memory_base);
            mmio_write(memory_base_reg_addr, bridge->memory_base, sizeof(uint16_t));

            uintptr_t memory_limit_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, memory_limit);
            mmio_write(memory_limit_reg_addr, bridge->memory_limit, sizeof(uint16_t));

            uintptr_t prefetchable_memory_base_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, prefetchable_memory_base);
            mmio_write(prefetchable_memory_base_reg_addr, bridge->prefetchable_memory_base, sizeof(uint16_t));

            uintptr_t prefetchable_memory_limit_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, prefetchable_memory_limit);
            mmio_write(prefetchable_memory_limit_reg_addr, bridge->prefetchable_memory_limit, sizeof(uint16_t));

            uintptr_t prefetchable_base_upper_32bits_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, prefetchable_base_upper_32bits);
            mmio_write(prefetchable_base_upper_32bits_reg_addr, bridge->prefetchable_base_upper_32bits, sizeof(uint32_t));

            uintptr_t prefetchable_limit_upper_32bits_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, prefetchable_limit_upper_32bits);
            mmio_write(prefetchable_limit_upper_32bits_reg_addr, bridge->prefetchable_limit_upper_32bits, sizeof(uint32_t));

            uintptr_t expension_rom_bar_offset      = offsetof_field(pci_pci2pci_bridge_t, expension_rom_base_address);
            size_t expension_rom_bar_u32_idx        = expension_rom_bar_offset / sizeof(uint32_t);
            uintptr_t expension_rom_bar_offset_addr = pci_hdr_addr + expension_rom_bar_offset;

            uint32_t expension_rom_bar_value = u32_data[expension_rom_bar_u32_idx];
            mmio_write(expension_rom_bar_offset_addr, expension_rom_bar_value, sizeof(uint32_t));

            uintptr_t interrupt_line_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, interrupt_line);
            mmio_write(interrupt_line_reg_addr, bridge->interrupt_line, sizeof(uint8_t));

            uintptr_t bridge_control_reg_addr = pci_hdr_addr + offsetof_field(pci_pci2pci_bridge_t, bridge_control);
            mmio_write(bridge_control_reg_addr, bridge->bridge_control, sizeof(uint16_t));
        }


        uint16_t command_reg = pchdr->command.value;
        mmio_write(command_reg_addr, command_reg, sizeof(uint16_t));

        PRINTLOG(PCI, LOG_INFO, "pci dev %02x:%02x:%02x.%02x registers restored",
                 p->group_number, p->bus_number, p->device_number, p->function_number);
    }

    return 0;
}
