#include <driver/virtio.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <video.h>
#include <time/timer.h>
#include <ports.h>
#include <apic.h>

int8_t virtio_create_queue(virtio_dev_t* vdev, uint16_t queue_no, uint64_t queue_item_size, boolean_t write, boolean_t iter_rw, virtio_queue_item_builder_f item_builder, interrupt_irq modern, interrupt_irq legacy) {
    PRINTLOG(VIRTIO, LOG_TRACE, "queue 0x%x size 0x%x", queue_no, vdev->queue_size);

    if(vdev->is_legacy) {
        outw(vdev->iobase + VIRTIO_IOPORT_VQ_SELECT, queue_no);
        time_timer_spinsleep(1000);
        outw(vdev->iobase + VIRTIO_IOPORT_VQ_SIZE, vdev->queue_size);
        time_timer_spinsleep(1000);
    } else {
        vdev->common_config->queue_select = queue_no;
        time_timer_spinsleep(1000);
        vdev->common_config->queue_size = vdev->queue_size;
        time_timer_spinsleep(1000);
    }

    uint64_t queue_size = vdev->queue_size * queue_item_size;
    uint64_t queue_meta_size = virtio_queue_get_size(vdev);

    uint64_t queue_frm_cnt = (queue_size + FRAME_SIZE - 1) / FRAME_SIZE;
    uint64_t queue_meta_frm_cnt = (queue_meta_size + FRAME_SIZE - 1 ) / FRAME_SIZE;


    frame_t* queue_frames = NULL;
    frame_t* queue_meta_frames = NULL;

    boolean_t do_not_init = !write;


    if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) == 0 &&
       KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, queue_meta_frm_cnt, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_meta_frames, NULL) == 0) {
        queue_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
        queue_meta_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

        uint64_t queue_fa = queue_frames->frame_address;
        uint64_t queue_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_frames->frame_address);
        memory_paging_add_va_for_frame(queue_va, queue_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        PRINTLOG(VIRTIO, LOG_TRACE, "queue 0x%x data is at fa 0x%llx va 0x%llx", queue_no, queue_fa, queue_va);

        uint64_t queue_meta_fa = queue_meta_frames->frame_address;
        uint64_t queue_meta_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(queue_meta_frames->frame_address);
        memory_paging_add_va_for_frame(queue_meta_va, queue_meta_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        virtio_queue_t vq = (virtio_queue_t)queue_meta_va;
        virtio_queue_descriptor_t* descs = virtio_queue_get_desc(vdev, vq);
        virtio_queue_avail_t* avail = virtio_queue_get_avail(vdev, vq);
        virtio_queue_used_t* used = virtio_queue_get_used(vdev, vq);

        vdev->queues[queue_no].vq = vq;

        uint16_t avail_idx = 0;

        for(int32_t i = 0; i < vdev->queue_size; i++) {
            descs[i].address = queue_fa + i * queue_item_size;


            if(do_not_init) {
                descs[i].length = 0;
            } else {
                descs[i].length = queue_item_size;
            }

            if(iter_rw) {
                if(write) {
                    descs[i].flags = write?VIRTIO_QUEUE_DESC_F_WRITE:0;
                    write = 0;
                } else {
                    descs[i].flags = VIRTIO_QUEUE_DESC_F_NEXT;
                    descs[i].next = i + 1;
                    write = 1;
                    avail->ring[avail_idx]  = i;
                    avail_idx++;
                }
            } else {
                descs[i].flags = write?VIRTIO_QUEUE_DESC_F_WRITE:0;
                avail->ring[avail_idx]  = avail_idx;
                avail_idx++;
            }

            if(item_builder) {
                item_builder(vdev, (void*)(descs[i].address));
            }

        }

        if(do_not_init) {
            avail->index = 0;
        } else {
            avail->index = avail_idx;
        }


        PRINTLOG(VIRTIO, LOG_TRACE, "queue 0x%x builded", queue_no);

        if(vdev->is_legacy) {
            outw(vdev->iobase + VIRTIO_IOPORT_VQ_SELECT, queue_no);
            time_timer_spinsleep(1000);
            outw(vdev->iobase + VIRTIO_IOPORT_VQ_SIZE, vdev->queue_size);
            time_timer_spinsleep(1000);
            outl(vdev->iobase + VIRTIO_IOPORT_VQ_ADDR, queue_meta_fa);
            time_timer_spinsleep(1000);

            if(vdev->has_msix) {
                if(modern) {
                    outw(vdev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, queue_no);
                    time_timer_spinsleep(1000);

                    while(inw(vdev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR) != queue_no) {
                        PRINTLOG(VIRTIO, LOG_WARNING, "queue 0x%x msix not configured re-trying", queue_no);
                        outw(vdev->iobase + VIRTIO_IOPORT_VQ_MSIX_VECTOR, queue_no);
                        time_timer_spinsleep(1000);
                    }

                    pci_msix_set_isr((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, queue_no, modern);
                }

            } else {
                if(legacy) {
                    uint8_t isrnum = vdev->irq_base;
                    interrupt_irq_set_handler(isrnum, legacy);
                    apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
                    apic_ioapic_enable_irq(isrnum);

                    PRINTLOG(VIRTIO, LOG_TRACE, "receive queue combined isr 0x%02x",  vdev->irq_base);
                }

            }

            outw(vdev->iobase + VIRTIO_IOPORT_VQ_NOTIFY, queue_no);
        } else {
            vdev->common_config->queue_select = queue_no;
            time_timer_spinsleep(1000);

            if(vdev->common_config->queue_size != vdev->queue_size) {
                PRINTLOG(VIRTIO, LOG_ERROR, "queue size missmatch 0x%x", vdev->common_config->queue_size);

                return -1;
            }

            vdev->common_config->queue_size = vdev->queue_size;
            time_timer_spinsleep(1000);
            vdev->common_config->queue_desc = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)descs));
            time_timer_spinsleep(1000);
            vdev->common_config->queue_driver = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)avail));
            time_timer_spinsleep(1000);
            vdev->common_config->queue_device = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(((uint64_t)used));
            time_timer_spinsleep(1000);

            vdev->common_config->queue_enable = 1;
            time_timer_spinsleep(1000);

            if(vdev->has_msix) {
                if(modern) {
                    vdev->common_config->queue_msix_vector = queue_no;
                    time_timer_spinsleep(1000);

                    while(vdev->common_config->queue_msix_vector != queue_no) {
                        PRINTLOG(VIRTIO, LOG_WARNING, "queue 0x%x msix not configured re-trying", queue_no);
                        vdev->common_config->queue_msix_vector = queue_no;
                        time_timer_spinsleep(1000);
                    }

                    pci_msix_set_isr((pci_generic_device_t*)vdev->pci_dev->pci_header, vdev->msix_cap, queue_no, modern);
                }
            } else {
                if(legacy) {
                    uint8_t isrnum = vdev->irq_base;
                    interrupt_irq_set_handler(isrnum, legacy);
                    apic_ioapic_setup_irq(isrnum, APIC_IOAPIC_TRIGGER_MODE_EDGE);
                    apic_ioapic_enable_irq(isrnum);

                    PRINTLOG(VIRTIO, LOG_TRACE, "queue 0x%x combined isr 0x%02x", queue_no, vdev->irq_base);
                }

            }

            // TODO: if nd not exists?

            virtio_notification_data_t* nd = (virtio_notification_data_t*)(vdev->common_config->queue_notify_offset * vdev->notify_offset_multiplier + vdev->notify_offset);

            vdev->queues[queue_no].nd = nd;

            nd->vqn = queue_no;

            PRINTLOG(VIRTIO, LOG_TRACE, "notify data at 0x%p for queue 0x%x is notified", nd, queue_no);
        }

        PRINTLOG(VIRTIO, LOG_TRACE, "queue 0x%x configured", queue_no);
    } else {
        PRINTLOG(VIRTIO, LOG_ERROR, "cannot allocate frames of queue 0x%x", queue_no);

        return -1;
    }

    return 0;
}


int8_t virtio_init_legacy(virtio_dev_t* vdev, virtio_select_features_f select_features, virtio_create_queues_f create_queues) {
    PRINTLOG(VIRTIO, LOG_TRACE, "legacy device addr 0x%x", vdev->iobase);

    outb(vdev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, 0);
    time_timer_spinsleep(1000);
    outb(vdev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE);
    time_timer_spinsleep(1000);
    outb(vdev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE | VIRTIO_DEVICE_STATUS_DRIVER);


    vdev->features = inl(vdev->iobase + VIRTIO_IOPORT_DEVICE_F);

    if(select_features) {
        vdev->selected_features = select_features(vdev, vdev->features);

        PRINTLOG(VIRTIO, LOG_TRACE, "features avail 0x%08llx req 0x%08llx", vdev->features, vdev->selected_features );

        outl(vdev->iobase + VIRTIO_IOPORT_DRIVER_F, vdev->selected_features);
        time_timer_spinsleep(1000);

        uint32_t new_avail_features = inl(vdev->iobase + VIRTIO_IOPORT_DRIVER_F);

        if(new_avail_features != vdev->selected_features) {
            PRINTLOG(VIRTIO, LOG_ERROR, "device doesnot accepted requested features, device offers: 0x%x", new_avail_features);

            return -1;
        }
    }

    PRINTLOG(VIRTIO, LOG_TRACE, "device accepted requested features");

    if(create_queues != NULL && create_queues(vdev) == 0) {
        outb(vdev->iobase + VIRTIO_IOPORT_DEVICE_STATUS, VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE | VIRTIO_DEVICE_STATUS_DRIVER | VIRTIO_DEVICE_STATUS_DRIVER_OK);
        time_timer_spinsleep(1000);
        uint8_t new_dev_status = inb(vdev->iobase + VIRTIO_IOPORT_DEVICE_STATUS);

        if(new_dev_status & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
            PRINTLOG(VIRTIO, LOG_TRACE, "device accepted ok status");

            return 0;
        } else {
            PRINTLOG(VIRTIO, LOG_ERROR, "device doesnot accepted ok status");
        }

    }


    return -1;
}

int8_t virtio_init_modern(virtio_dev_t* vdev, virtio_select_features_f select_features, virtio_create_queues_f create_queues){
    PRINTLOG(VIRTIO, LOG_TRACE, "modern device addr 0x%p", vdev->common_config);

    vdev->common_config->device_status = 0;
    time_timer_spinsleep(1000);
    vdev->common_config->device_status = VIRTIO_DEVICE_STATUS_ACKKNOWLEDGE;
    time_timer_spinsleep(1000);
    vdev->common_config->device_status |= VIRTIO_DEVICE_STATUS_DRIVER;
    time_timer_spinsleep(1000);

    vdev->common_config->device_feature_select = 1;
    time_timer_spinsleep(1000);
    uint64_t avail_features = vdev->common_config->device_feature;
    avail_features <<= 32;
    vdev->common_config->device_feature_select = 0;
    time_timer_spinsleep(1000);
    avail_features |= vdev->common_config->device_feature;



    vdev->features = avail_features;

    if(select_features) {
        vdev->selected_features = select_features(vdev, vdev->features);

        if(avail_features & VIRTIO_F_IN_ORDER) {
            vdev->selected_features |= VIRTIO_F_IN_ORDER;
        }

        PRINTLOG(VIRTIO, LOG_TRACE, "features avail 0x%llx req 0x%llx", vdev->features, vdev->selected_features );

        vdev->common_config->driver_feature_select = 1;
        time_timer_spinsleep(1000);
        vdev->common_config->driver_feature = vdev->selected_features >> 32;
        time_timer_spinsleep(1000);
        vdev->common_config->driver_feature_select = 0;
        time_timer_spinsleep(1000);
        vdev->common_config->driver_feature = vdev->selected_features;
        time_timer_spinsleep(1000);
        vdev->common_config->device_status |= VIRTIO_DEVICE_STATUS_FEATURES_OK;
        time_timer_spinsleep(1000);
        uint8_t new_dev_status = vdev->common_config->device_status;


        if((new_dev_status & VIRTIO_DEVICE_STATUS_FEATURES_OK) != VIRTIO_DEVICE_STATUS_FEATURES_OK) {
            PRINTLOG(VIRTIO, LOG_ERROR, "device doesnot accepted requested features");

            return -1;
        }
    }

    PRINTLOG(VIRTIO, LOG_TRACE, "device accepted requested features");

    if(create_queues != NULL && create_queues(vdev) == 0) {
        PRINTLOG(VIRTIO, LOG_TRACE, "try to set driver ok");

        vdev->common_config->device_status |= VIRTIO_DEVICE_STATUS_DRIVER_OK;
        time_timer_spinsleep(1000);
        uint8_t new_dev_status = vdev->common_config->device_status;

        if(new_dev_status & VIRTIO_DEVICE_STATUS_DRIVER_OK) {
            PRINTLOG(VIRTIO, LOG_TRACE, "device accepted ok status");

            return 0;
        } else {
            PRINTLOG(VIRTIO, LOG_ERROR, "device doesnot accepted ok status");
        }

    }

    return -1;
}

virtio_dev_t* virtio_get_device(pci_dev_t* pci_dev) {
    virtio_dev_t* vdev = memory_malloc(sizeof(virtio_dev_t));

    if(vdev == NULL) {
        return vdev;
    }

    vdev->return_queue = linkedlist_create_list();

    if(vdev->return_queue == NULL) {
        memory_free(vdev);

        return NULL;
    }

    pci_common_header_t* pci_header = pci_dev->pci_header;
    pci_generic_device_t* pci_gen_dev = (pci_generic_device_t*)pci_header;

    vdev->pci_dev = pci_dev;


    vdev->is_legacy = 1;

    if(pci_gen_dev->common_header.status.capabilities_list) {
        pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_gen_dev) + pci_gen_dev->capabilities_pointer);

        while(pci_cap->capability_id != 0xFF) {
            PRINTLOG(VIRTIO, LOG_TRACE, "capability id: %i noff 0x%x", pci_cap->capability_id, pci_cap->next_pointer);

            if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                pci_capability_msix_t* msix_cap = (pci_capability_msix_t*)pci_cap;

                vdev->msix_cap = msix_cap;

                msix_cap->enable = 1;
                msix_cap->function_mask = 0;

                PRINTLOG(VIRTIO, LOG_TRACE, "device has msix cap enabled %i fmask %i", msix_cap->enable, msix_cap->function_mask);
                PRINTLOG(VIRTIO, LOG_TRACE, "msix bir %i tables offset 0x%x  size 0x%x", msix_cap->bir, msix_cap->table_offset, msix_cap->table_size + 1);
                PRINTLOG(VIRTIO, LOG_TRACE, "msix pending bit bir %i tables offset 0x%x", msix_cap->pending_bit_bir, msix_cap->pending_bit_offset);

                vdev->has_msix = 1;
            } else if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_VENDOR) {
                vdev->is_legacy = 0;
                virtio_pci_cap_t* vcap = (virtio_pci_cap_t*)pci_cap;

                PRINTLOG(VIRTIO, LOG_TRACE, "virtio cap type 0x%02x bar %i off 0x%x len 0x%x", vcap->config_type, vcap->bar_no, vcap->offset, vcap->length);

                pci_bar_register_t* bar = &pci_gen_dev->bar0;

                bar += vcap->bar_no;

                if(bar->bar_type.type == 0) {
                    uint64_t bar_fa = pci_get_bar_address(pci_gen_dev, vcap->bar_no);

                    PRINTLOG(VIRTIO, LOG_TRACE, "frame address at bar 0x%llx", bar_fa);

                    frame_t* bar_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)bar_fa);

                    uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

                    if(bar_frames == NULL) {
                        PRINTLOG(VIRTIO, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);
                        uint64_t size = pci_get_bar_size(pci_gen_dev, vcap->bar_no);
                        uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
                        frame_t tmp_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

                        if(KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &tmp_frm) != 0) {
                            PRINTLOG(VIRTIO, LOG_ERROR, "cannot allocate frame");
                            memory_free(vdev);

                            return NULL;
                        }

                        bar_frames = &tmp_frm;
                    }

                    if((bar_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_frames->frame_address), bar_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
                        bar_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                    }

                    switch (vcap->config_type) {
                    case VIRTIO_PCI_CAP_COMMON_CFG:
                        vdev->common_config = (virtio_pci_common_config_t*)(bar_va + vcap->offset);
                        PRINTLOG(VIRTIO, LOG_TRACE, "common config address 0x%p", vdev->common_config);
                        break;
                    case VIRTIO_PCI_CAP_NOTIFY_CFG:
                        vdev->notify_offset_multiplier = ((virtio_pci_notify_cap_t*)vcap)->notify_offset_multiplier;
                        vdev->notify_offset = (uint8_t*)(bar_va + vcap->offset);
                        PRINTLOG(VIRTIO, LOG_TRACE, "notify multipler 0x%x address 0x%p", vdev->notify_offset_multiplier, vdev->notify_offset);
                        break;
                    case VIRTIO_PCI_CAP_ISR_CFG:
                        vdev->isr_config = (void*)(bar_va + vcap->offset);
                        PRINTLOG(VIRTIO, LOG_TRACE, "isr config address 0x%p", vdev->isr_config);
                        break;
                    case VIRTIO_PCI_CAP_DEVICE_CFG:
                        vdev->device_config = (void*)(bar_va + vcap->offset);
                        PRINTLOG(VIRTIO, LOG_TRACE, "device config address 0x%p", vdev->device_config);
                        break;
                    case VIRTIO_PCI_CAP_PCI_CFG:
                        break;
                    default:
                        break;
                    }


                } else {
                    PRINTLOG(VIRTIO, LOG_ERROR, "unexpected io space bar");
                }

            } else {
                PRINTLOG(VIRTIO, LOG_WARNING, "not implemented cap 0x%02x", pci_cap->capability_id);
            }

            if(pci_cap->next_pointer == NULL) {
                break;
            }

            pci_cap = (pci_capability_t*)(((uint8_t*)pci_gen_dev) + pci_cap->next_pointer);
        }
    }

    if(!vdev->has_msix) {
        PRINTLOG(VIRTIO, LOG_TRACE, "device hasnot msi looking for interrupts at acpi");
        uint8_t ic;

        uint64_t addr = pci_dev->device_number;
        addr <<= 16;

        addr |= pci_dev->function_number;

        uint8_t* ints = acpi_device_get_interrupts(ACPI_CONTEXT->acpi_parser_context, addr, &ic);

        if(ic == 0) { // need look with function as 0xFFFF
            addr |= 0xFFFF;
            ints = acpi_device_get_interrupts(ACPI_CONTEXT->acpi_parser_context, addr, &ic);
        }

        if(ic) {
            PRINTLOG(VIRTIO, LOG_TRACE, "device has %i interrupts, will use pina: 0x%02x", ic, ints[ic - 1]);

            vdev->irq_base = ints[ic - 1];

            memory_free(ints);
        } else {
            PRINTLOG(VIRTIO, LOG_ERROR, "device hasnot any interrupts");
            memory_free(vdev);

            return NULL;
        }
    }

    if(vdev->is_legacy) {
        vdev->iobase = pci_gen_dev->bar0.io_space_bar.base_address << 2;

        PRINTLOG(VIRTIO, LOG_TRACE, "device is legacy iobase: 0x%04x",  vdev->iobase);
    }

    return vdev;
}

int8_t virtio_init_dev(virtio_dev_t* vdev, virtio_select_features_f select_features, virtio_create_queues_f create_queues) {
    if(vdev->is_legacy) {

        return virtio_init_legacy(vdev, select_features, create_queues);
    }

    return virtio_init_modern(vdev, select_features, create_queues);

}
