/**
 * @file nvme.h
 * @brief nvme interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/nvme.h>
#include <pci.h>
#include <video.h>
#include <logging.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <time/timer.h>


MODULE("turnstone.kernel.hw.drivers");

int8_t nvme_init(memory_heap_t* heap, linkedlist_t nvme_pci_devices) {
    PRINTLOG(NVME, LOG_INFO, "disk searching started");

    if(linkedlist_size(nvme_pci_devices) == 0) {
        PRINTLOG(NVME, LOG_WARNING, "no SATA devices");
        return 0;
    }

    UNUSED(heap);

    uint64_t disk_id = 0;

    iterator_t* iter = linkedlist_iterator_create(nvme_pci_devices);

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* p = iter->get_item(iter);
        pci_generic_device_t* pci_nvme = (pci_generic_device_t*)p->pci_header;


        pci_capability_msi_t* msi_cap = NULL;
        pci_capability_msix_t* msix_cap = NULL;

        if(pci_nvme->common_header.status.capabilities_list) {
            pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_nvme) + pci_nvme->capabilities_pointer);


            while(pci_cap->capability_id != 0xFF) {
                if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSI) {
                    msi_cap = (pci_capability_msi_t*)pci_cap;
                } if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSIX) {
                    msix_cap = (pci_capability_msix_t*)pci_cap;
                }else {
                    PRINTLOG(NVME, LOG_WARNING, "not implemented cap 0x%02x", pci_cap->capability_id);
                }

                if(pci_cap->next_pointer == NULL) {
                    break;
                }

                pci_cap = (pci_capability_t*)(((uint8_t*)pci_nvme) + pci_cap->next_pointer);
            }
        }

        uint64_t bar_fa = pci_get_bar_address(pci_nvme, 0);

        PRINTLOG(NVME, LOG_TRACE, "frame address at bar 0x%llx", bar_fa);

        frame_t* bar_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)bar_fa);

        uint64_t bar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(bar_fa);

        if(bar_frames == NULL) {
            PRINTLOG(NVME, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", bar_fa);
            uint64_t size = pci_get_bar_size(pci_nvme, 0);
            uint64_t bar_frm_cnt = (size + FRAME_SIZE - 1) / FRAME_SIZE;
            frame_t tmp_frm = {bar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

            if(KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &tmp_frm) != 0) {
                PRINTLOG(NVME, LOG_ERROR, "cannot allocate frame");

                return 0;
            }

            bar_frames = &tmp_frm;
        }

        if((bar_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
            memory_paging_add_va_for_frame(bar_va, bar_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
            bar_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
        }

        nvme_controller_registers_t* nvme_regs = (nvme_controller_registers_t*)bar_va;

        PRINTLOG(NVME, LOG_DEBUG, "nvme controller %lli ready? %i has msi? %i has_msix? %i", disk_id, nvme_regs->status.ready, msi_cap != NULL, msix_cap != NULL);
        PRINTLOG(NVME, LOG_DEBUG, "nvme version %i.%i.%i", nvme_regs->version.major, nvme_regs->version.minor, nvme_regs->version.reserved_or_ter);

        frame_t* queue_frames = NULL;

        if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, 4, FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED, &queue_frames, NULL) != 0) {
            PRINTLOG(NVME, LOG_ERROR, "cannot allocate frame for queues");

            return 0;
        }

        nvme_regs->config.enable = 0;

        do {
            time_timer_spinsleep(500 * (nvme_regs->capabilities.fields.timeout + 1));
        } while(nvme_regs->status.ready);

        if(nvme_regs->status.cfs != 0) {
            PRINTLOG(NVME, LOG_ERROR, "error at disabling nvme: %i", nvme_regs->status.cfs);

            return 0;
        }

        PRINTLOG(NVME, LOG_DEBUG, "nvme asq %llx acq %llx", queue_frames->frame_address, queue_frames->frame_address + FRAME_SIZE);
        //nvme_regs->config.css = 7;
        nvme_regs->asq = queue_frames->frame_address;
        nvme_regs->acq = queue_frames->frame_address + FRAME_SIZE;
        nvme_regs->aqa.acqs = 63;
        nvme_regs->aqa.asqs = 63;
        nvme_regs->config.iosqes = 6;
        nvme_regs->config.iocqes = 4;
        nvme_regs->config.ams = 0;
        nvme_regs->config.mps = 0;
        nvme_regs->intms = 0xFFFFFFFF;

        nvme_regs->config.enable = 1;

        do {
            time_timer_spinsleep(500 * (nvme_regs->capabilities.fields.timeout + 1));

            if(nvme_regs->status.cfs) {
                break;
            }
        } while(!nvme_regs->status.ready);

        if(nvme_regs->status.ready != 1) {
            PRINTLOG(NVME, LOG_ERROR, "cannot enable nvme: %i", nvme_regs->status.cfs);

            return 0;
        }

        disk_id++;
        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

