/**
 * @file ahci.64.c
 * @brief ahci interface implentation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/ahci.h>
#include <logging.h>
#include <pci.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <apic.h>
#include <cpu.h>
#include <time/timer.h>
#include <utils.h>

MODULE("turnstone.kernel.hw.disk.ahci");

void video_text_print(const char* str);

list_t* sata_ports = NULL;
list_t* sata_hbas = NULL;

ahci_device_type_t ahci_check_type(ahci_hba_port_t* port);
int8_t             ahci_find_command_slot(ahci_sata_disk_t* disk);
void               ahci_port_rebase(ahci_hba_port_t* port, uint64_t offset, int8_t nr_cmd_slots);
void               ahci_port_start_cmd(ahci_hba_port_t* port);
void               ahci_port_stop_cmd(ahci_hba_port_t* port);
void               ahci_handle_disk_isr(const ahci_hba_t* hba, uint64_t disk_id);
int8_t             ahci_error_recovery_ncq(ahci_sata_disk_t* disk);
int8_t             ahci_read_log_ncq(ahci_sata_disk_t* disk);
int8_t             ahci_port_comreset(ahci_hba_port_t* port);
int8_t             ahci_disk_id_comparator(const void* disk1, const void* disk2);

int8_t ahci_isr(interrupt_frame_ext_t* frame);

const ahci_sata_disk_t* ahci_get_disk_by_id(uint64_t disk_id) {
    ahci_sata_disk_t tmp_disk;
    tmp_disk.disk_id = disk_id;
    uint64_t pos;

    if(list_get_position(sata_ports, &tmp_disk, &pos) != 0) {
        return NULL;
    }

    return list_get_data_at_position(sata_ports, pos);
}

const ahci_sata_disk_t* ahci_get_first_inserted_disk(void) {
    iterator_t* iter = list_iterator_create(sata_ports);

    if(iter == NULL) {
        return NULL;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const ahci_sata_disk_t* disk = iter->get_item(iter);

        if(disk->inserted) {
            iter->destroy(iter);

            return disk;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return NULL;
}

int8_t ahci_isr(interrupt_frame_ext_t* frame){
    uint8_t intnum = frame->interrupt_number;
    intnum -= 0x20;

    boolean_t irq_handled = 0;

    for(uint64_t disk_idx = 0; disk_idx < list_size(sata_hbas); disk_idx++) {
        const ahci_hba_t* hba = list_get_data_at_position(sata_hbas, disk_idx);

        if(hba->intnum_base <= intnum && intnum < (hba->intnum_base + hba->intnum_count)) {
            ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)hba->hba_addr;

            if(hba->intnum_count == 1) {
                uint32_t is = hba_mem->interrupt_status;

                for(uint8_t i = 0; i < hba->disk_count; i++) {
                    if(is & 1) {
                        ahci_handle_disk_isr(hba, hba->disk_base + i);
                    }

                    is >>= 1;
                }

            } else {
                ahci_handle_disk_isr(hba, hba->disk_base + intnum - hba->intnum_base);
            }

            irq_handled = true;

            break;
        }
    }

    if(irq_handled) {
        apic_eoi();

        return 0;
    }

    return -1;
}

void ahci_handle_disk_isr(const ahci_hba_t* hba, uint64_t disk_id) {
    ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)hba->hba_addr;
    ahci_sata_disk_t* disk = (ahci_sata_disk_t*)list_get_data_at_position(sata_ports, disk_id);
    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    // PRINTLOG(AHCI, LOG_TRACE, "interrupt status 0x%x cmd_status 0x%x sactive 0x%x serror 0x%x cmdissued 0x%x tfd 0x%x",
    // port->interrupt_status, port->command_and_status, port->sata_active, port->sata_error, port->command_issue, port->task_file_data);

    uint32_t finished_commands = disk->current_commands ^ port->command_issue;

    for(uint32_t i = 0; i < disk->command_count; i++) {
        if(bit_test32(&finished_commands, i)) {
            // PRINTLOG(AHCI, LOG_TRACE, "command %i finished for disk %lli", i, disk_id);

            if(disk->future_locks[i]) {
                // PRINTLOG(AHCI, LOG_TRACE, "releasing future lock for command %i for disk %lli", i, disk_id);
                lock_release(disk->future_locks[i]);
                disk->future_locks[i] = NULL;
            }

            bit_clear32(&disk->current_commands, i);
            bit_clear32(&disk->acquired_slots, i);
        }
    }

    if(port->task_file_data & 1) {
        if(port->sata_active) {
            ahci_error_recovery_ncq(disk);
        }
    }

    port->interrupt_status = (uint32_t)-1;
    hba_mem->interrupt_status = 1;
}

int8_t ahci_port_comreset(ahci_hba_port_t* port){
    PRINTLOG(AHCI, LOG_TRACE, "comreset started 0x%x 0x%x", port->sata_control, port->sata_status);

    port->sata_control |= 1;

    time_timer_spinsleep(1000 * 1000);

    port->sata_control &= ~1;

    uint8_t try_cnt = 16;

    while((port->sata_status & 0xF) != 0x3 && try_cnt--) {
        time_timer_spinsleep(1000 * 1000 * 50);
    }

    port->sata_error = (uint32_t)-1;

    PRINTLOG(AHCI, LOG_TRACE, "comreset ended");

    return 0;
}

int8_t ahci_error_recovery_ncq(ahci_sata_disk_t* disk){
    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;
    port->command_and_status &= ~AHCI_HBA_PxCMD_ST;

    while(1) {
        if (port->command_and_status & AHCI_HBA_PxCMD_CR) { // loop while command running
            continue;
        }

        break;
    }

    port->sata_error = (uint32_t)-1;
    port->interrupt_status = (uint32_t)-1;

    port->command_and_status |= AHCI_HBA_PxCMD_ST; // set command start

    if(disk->logging.fields.gpl_enabled) {
        port->interrupt_enable = 0;
        port->interrupt_status = (uint32_t)-1;

        ahci_read_log_ncq(disk);

        port->interrupt_status = (uint32_t)-1;
        port->interrupt_enable = (uint32_t)-1;
    }

    if(port->sata_active) {
        PRINTLOG(AHCI, LOG_WARNING, "force comreset");
        ahci_port_comreset(port);
    }

    return 0;
}

int8_t ahci_read_log_ncq(ahci_sata_disk_t* disk) {
    uint8_t from_dma = disk->logging.fields.dma_ext_is_log_ext;
    ahci_ata_ncq_error_log_t error_log;
    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    int8_t slot = ahci_find_command_slot(disk);

    if(slot == -1) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot find empty command slot");
        return -1;
    }

    PRINTLOG(AHCI, LOG_TRACE, "slot %i", slot);

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address);
    cmd_hdr += slot;

    cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->write_direction = 0;
    cmd_hdr->prdt_length = 1;
    cmd_hdr->clear_busy = 1;

    ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_hdr->prdt_base_address);
    memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

    uint64_t el_phy_addr = 0;

    if(memory_paging_get_physical_address((uint64_t)&error_log, &el_phy_addr) != 0) {
        PRINTLOG(AHCI, LOG_ERROR, "cannot get physical address of error log 0x%p", &error_log);
        bit_clear32(&disk->acquired_slots, slot);

        return -1;
    }

    cmd_table->prdt_entry[0].data_base_address = el_phy_addr;
    cmd_table->prdt_entry[0].data_byte_count = sizeof(ahci_ata_ncq_error_log_t) - 1;

    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(&cmd_table->command_fis);
    memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->control_or_command = 1;

    if(from_dma) {
        fis->command = AHCI_ATA_CMD_READ_LOG_DMA_EXT;
    } else {
        fis->command = AHCI_ATA_CMD_READ_LOG_EXT;
    }

    fis->command = 0xb0;
    fis->featurel = 0xd5;

    fis->lba0 = 0xc24f00 | 0x10;
    fis->lba1 = 0;
    fis->count = 1;

    port->command_issue = 1 << slot;

    while(1) {
        if((port->command_issue & (1 << slot)) == 0) {
            break;
        }

        if(port->interrupt_status & AHCI_HBA_PxIS_TFES) {
            PRINTLOG(AHCI, LOG_FATAL, "read ncq log error is 0x%x tfd 0x%x err 0x%x", port->interrupt_status, port->task_file_data, port->sata_error);
            bit_clear32(&disk->acquired_slots, slot);

            return -1;
        }
    }

    bit_clear32(&disk->acquired_slots, slot);

    if(port->interrupt_status & 1) {
        ahci_hba_fis_t* hba_fis = (ahci_hba_fis_t*)port->fis_base_address;
        PRINTLOG(AHCI, LOG_TRACE, "hba fis lba %x count %x err %x status %x",
                 hba_fis->d2h_fis.lba0 | ( hba_fis->d2h_fis.lba1 << 24), hba_fis->d2h_fis.count, hba_fis->d2h_fis.error, hba_fis->d2h_fis.status );
    }

    PRINTLOG(AHCI, LOG_ERROR, "is 0x%x 0x%x 0x%x ncq 0x%x unload 0x%x tag 0x%x status 0x%x error 0x%x lba 0x%x count 0x%x device 0x%x ",
             port->interrupt_status, port->task_file_data, port->sata_error,
             error_log.nq, error_log.unload, error_log.ncq_tag,
             error_log.status, error_log.error, error_log.lba0 | (error_log.lba1 << 24), error_log.count, error_log.device);

    return 0;
}

int8_t ahci_disk_id_comparator(const void* disk1, const void* disk2) {
    uint64_t disk1_id = ((ahci_sata_disk_t*)disk1)->disk_id;
    uint64_t disk2_id = ((ahci_sata_disk_t*)disk2)->disk_id;

    if(disk1_id < disk2_id) {
        return -1;
    }

    if(disk1_id > disk2_id) {
        return 1;
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t ahci_init(memory_heap_t* heap, list_t* sata_pci_devices) {
    PRINTLOG(AHCI, LOG_INFO, "ahci controller init started");

    if(list_size(sata_pci_devices) == 0) {
        PRINTLOG(AHCI, LOG_WARNING, "no SATA devices");
        return 0;
    }

    uint64_t disk_id = 0;

    sata_ports = list_create_sortedlist_with_heap(heap, ahci_disk_id_comparator);
    sata_hbas = list_create_list_with_heap(heap);

    iterator_t* iter = list_iterator_create(sata_pci_devices);

    while(iter->end_of_iterator(iter) != 0) {
        const pci_dev_t* p = iter->get_item(iter);
        pci_generic_device_t* pci_sata = (pci_generic_device_t*)p->pci_header;

        pci_disable_interrupt(pci_sata);


        pci_capability_msi_t* msi_cap = NULL;
        ahci_pci_capability_sata_t* sata_cap = NULL;

        if(pci_sata->common_header.status.capabilities_list) {
            pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_sata) + pci_sata->capabilities_pointer);


            while(pci_cap->capability_id != 0xFF) {
                if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSI) {
                    msi_cap = (pci_capability_msi_t*)pci_cap;
                } else if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_SATA) {
                    sata_cap = (ahci_pci_capability_sata_t*)pci_cap;
                } else {
                    PRINTLOG(AHCI, LOG_WARNING, "not implemented cap 0x%02x", pci_cap->capability_id);
                }

                if(pci_cap->next_pointer == NULL) {
                    break;
                }

                pci_cap = (pci_capability_t*)(((uint8_t*)pci_sata) + pci_cap->next_pointer);
            }
        }

        uint32_t abar_fa_tmp = (uint32_t)(pci_sata->bar5.memory_space_bar.base_address << 4);
        uint64_t abar_fa = (uint64_t)abar_fa_tmp;

        if(pci_sata->bar5.memory_space_bar.base_address == 2) {
            PRINTLOG(AHCI, LOG_TRACE, "64 bit abar");

            pci_bar_register_t* bar = &pci_sata->bar5;
            bar++;
            uint64_t tmp = (uint64_t)(*((uint32_t*)bar));

            abar_fa = tmp << 32 | abar_fa;
        }


        PRINTLOG(AHCI, LOG_TRACE, "frame address at bar 0x%llx", abar_fa);

        frame_t* bar_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)abar_fa);
        uint64_t bar_frm_cnt = 2;
        frame_t bar_req_frm = {abar_fa, bar_frm_cnt, FRAME_TYPE_RESERVED, 0};

        uint64_t abar_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(abar_fa);

        if(bar_frames == NULL) {
            PRINTLOG(AHCI, LOG_TRACE, "cannot find reserved frames for 0x%llx and try to reserve", abar_fa);

            if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &bar_req_frm) != 0) {
                PRINTLOG(AHCI, LOG_ERROR, "cannot allocate frame");

                return -1;
            }
        }

        memory_paging_add_va_for_frame(abar_va, &bar_req_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)abar_va;

        hba_mem->bios_os_handoff_control_and_status = 2;
        PRINTLOG(AHCI, LOG_TRACE, "hba global host control value 0x%x bohc 0x%x", hba_mem->global_host_control, hba_mem->bios_os_handoff_control_and_status );

        hba_mem->global_host_control = (1U << 31);

        PRINTLOG(AHCI, LOG_TRACE, "hba global host control value 0x%x", hba_mem->global_host_control );

        uint8_t nr_cmd_slots = ( (hba_mem->host_capability >> 8) & 0x1F) + 1;
        uint8_t nr_port = (hba_mem->host_capability & 0x1F) + 1;
        uint8_t sncq  = (hba_mem->host_capability >> 30) & 1;

        ahci_hba_t* hba = memory_malloc_ext(heap, sizeof(ahci_hba_t), 0x0);

        if(hba == NULL) {
            iter->destroy(iter);

            return -1;
        }

        hba->hba_addr = abar_va;
        hba->disk_base = disk_id;
        hba->disk_count = nr_port;

        if(sata_cap) {
            hba->revision_major = sata_cap->revision_major;
            hba->revision_minor = sata_cap->revision_minor;
            hba->sata_bar = sata_cap->bar_location - 4;
            hba->sata_bar_offset = sata_cap->bar_offset << 2;

            PRINTLOG(AHCI, LOG_TRACE, "sata revision %i.%i sata bar %i offset 0x%x", hba->revision_major, hba->revision_minor, hba->sata_bar, hba->sata_bar_offset );
        }

        list_list_insert(sata_hbas, hba);

        if(msi_cap) {
            PRINTLOG(AHCI, LOG_TRACE, "msi capability present");

            uint8_t msg_count = 1 << msi_cap->multiple_message_count;

            PRINTLOG(AHCI, LOG_TRACE, "ma64 support %i vector masking %i msg count %i", msi_cap->ma64_support, msi_cap->per_vector_masking, msg_count);

            uint8_t intnum = interrupt_get_next_empty_interrupt();
            hba->intnum_base = intnum - INTERRUPT_IRQ_BASE;
            hba->intnum_count = msg_count;

            uint32_t msg_addr = 0xFEE00000;
            uint32_t apic_id = apic_get_local_apic_id();
            apic_id <<= 12;
            msg_addr |= apic_id;


            if(msi_cap->ma64_support) {
                msi_cap->ma64.message_address = msg_addr; // | (1 << 3) | (0 << 2);
                msi_cap->ma64.message_data = intnum;
            } else {
                msi_cap->ma32.message_address = msg_addr; // | (1 << 3) | (0 << 2);
                msi_cap->ma32.message_data = intnum;
            }

            for(uint8_t i = 0; i < msg_count; i++) {
                uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
                interrupt_irq_set_handler(isrnum, &ahci_isr);
                PRINTLOG(AHCI, LOG_TRACE, "interrupt 0x%x registered as isr 0x%x for ahci_isr", intnum, isrnum);
                intnum = interrupt_get_next_empty_interrupt();
            }

            msi_cap->enable = 1;

        } else {
            PRINTLOG(AHCI, LOG_ERROR, "no msi capability");
            iter->destroy(iter);

            return -1;
        }

        PRINTLOG(AHCI, LOG_TRACE, "controller port cnt %i cmd slots %i sncq %i bohc %x", nr_port, nr_cmd_slots, sncq, hba_mem->bios_os_handoff_control_and_status);

        for(uint8_t port_idx = 0; port_idx < nr_port; port_idx++) {
            PRINTLOG(AHCI, LOG_TRACE, "checking port %i at ahci bar 0x%llx", port_idx, abar_va);

            uint64_t port_address = (uint64_t)&hba_mem->ports[port_idx];
            PRINTLOG(AHCI, LOG_TRACE, "port address 0x%llx", port_address);

            ahci_hba_port_t* port = (ahci_hba_port_t*)port_address;

            ahci_device_type_t dt = ahci_check_type(port);

            PRINTLOG(AHCI, LOG_TRACE, "port type 0x%x", dt);

            ahci_sata_disk_t* disk = memory_malloc_ext(heap, sizeof(ahci_sata_disk_t), 0x0);

            if(disk == NULL) {
                PRINTLOG(AHCI, LOG_ERROR, "cannot allocate memory for disk %lli at 0x%llx", disk_id, port_address);

                continue;
            }

            disk->heap = heap;
            disk->disk_id = disk_id++;
            disk->port_address = port_address;
            disk->type = dt;
            disk->sncq = sncq;
            disk->command_count = nr_cmd_slots;
            disk->disk_lock = lock_create_with_heap(heap);

            list_list_insert(sata_ports, disk);

            PRINTLOG(AHCI, LOG_DEBUG, "Disk %lli inserted for port %d", disk->disk_id, port_idx);

            if (dt == AHCI_DEVICE_SATA) {
                PRINTLOG(AHCI, LOG_DEBUG, "SATA drive found at port %d at 0x%llx", port_idx, port_address);

                frame_t* port_frames = NULL;

                if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(), 10, FRAME_ALLOCATION_TYPE_RESERVED | FRAME_ALLOCATION_TYPE_BLOCK, &port_frames, NULL) != 0) {
                    PRINTLOG(AHCI, LOG_ERROR, "cannot allocate frames for disk %lli at 0x%llx", disk->disk_id, disk->port_address);
                    iter->destroy(iter);

                    return -1;
                }

                if(memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port_frames->frame_address), port_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(AHCI, LOG_ERROR, "cannot page map frames for disk %lli at 0x%llx", disk->disk_id, disk->port_address);
                    iter->destroy(iter);

                    return -1;
                }

                memory_memclean((void*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port_frames->frame_address), 4096 * 10);

                ahci_port_rebase(port, port_frames->frame_address, nr_cmd_slots);

                if(ahci_identify(disk->disk_id) != 0) {
                    PRINTLOG(AHCI, LOG_ERROR, "cannot identify disk %lli at port address %llx", disk_id, port_address);
                }

                disk->inserted = true;

            }   else if (dt == AHCI_DEVICE_SATAPI)  {
                PRINTLOG(AHCI, LOG_DEBUG, "SATAPI drive found at port %d", port_idx);
            }   else if (dt == AHCI_DEVICE_SEMB)  {
                PRINTLOG(AHCI, LOG_DEBUG, "SEMB drive found at port %d", port_idx);
            }   else if (dt == AHCI_DEVICE_PM) {
                PRINTLOG(AHCI, LOG_DEBUG, "PM drive found at port %d", port_idx);
            }   else {
                PRINTLOG(AHCI, LOG_DEBUG, "No drive found at port %d", port_idx);
            }

        }

        hba_mem->global_host_control = (1 << 1); // enable interrupts

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    uint64_t disk_cnt = list_size(sata_ports);

    PRINTLOG(AHCI, LOG_INFO, "disk searching ended, %lli disks found", disk_cnt);

    return disk_cnt;
}
#pragma GCC diagnostic pop

future_t* ahci_flush(uint64_t disk_id) {
    ahci_sata_disk_t* disk = (ahci_sata_disk_t*)list_get_data_at_position(sata_ports, disk_id);

    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    int32_t slot = ahci_find_command_slot(disk);

    if(slot == -1) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot find empty command slot");
        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "flush port 0x%p slot %i", port, slot);

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address);
    cmd_hdr += slot;

    cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->write_direction = 0;
    cmd_hdr->prdt_length = 0;
    cmd_hdr->clear_busy = 1;

    ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_hdr->prdt_base_address);
    memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t));

    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(&cmd_table->command_fis);
    memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->control_or_command = 1;
    fis->command = AHCI_ATA_CMD_FLUSH_EXT;

    uint64_t tid = task_get_id();

    disk->future_locks[slot] = lock_create_with_heap_for_future(disk->heap, true, tid);

    future_t* fut = future_create_with_heap_and_data(disk->heap, disk->future_locks[slot], NULL);

    bit_set32(&disk->current_commands, slot);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    bit_set32(&port->command_issue, slot);
#pragma GCC diagnostic pop

    return fut;
}

int8_t ahci_identify(uint64_t disk_id) {
    ahci_sata_disk_t* disk = (ahci_sata_disk_t*)list_get_data_at_position(sata_ports, disk_id);

    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    ahci_ata_identify_data_t identify_data;

    port->interrupt_status = (uint32_t)-1;
    port->sata_error = (uint32_t)-1;

    int8_t slot = ahci_find_command_slot(disk);

    if(slot == -1) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot find empty command slot");
        return -1;
    }

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)port->command_list_base_address);
    cmd_hdr += slot;

    PRINTLOG(AHCI, LOG_TRACE, "identify port 0x%p slot %i cmd hdr 0x%p", port, slot, cmd_hdr);

    cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->write_direction = 0;
    cmd_hdr->prdt_length = 1;
    cmd_hdr->clear_busy = 1;

    ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)cmd_hdr->prdt_base_address);

    PRINTLOG(AHCI, LOG_TRACE, "cmd table 0x%p", cmd_table);

    memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

    uint64_t id_phy_addr = 0;

    if(memory_paging_get_physical_address((uint64_t)&identify_data, &id_phy_addr) != 0) {
        PRINTLOG(AHCI, LOG_ERROR, "cannot find physical address of buffer 0x%p", &identify_data);
        bit_clear32(&disk->acquired_slots, slot);

        return -1;
    }

    PRINTLOG(AHCI, LOG_TRACE, "id 0x%p phy addr 0x%llx", &identify_data, id_phy_addr);

    cmd_table->prdt_entry[0].data_base_address = id_phy_addr;
    cmd_table->prdt_entry[0].data_byte_count = sizeof(ahci_ata_identify_data_t) - 1;

    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(&cmd_table->command_fis);

    PRINTLOG(AHCI, LOG_TRACE, "fis 0x%p", fis);

    memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->control_or_command = 1;
    fis->command = AHCI_ATA_CMD_IDENTIFY;

    port->command_issue = 1 << slot;

    while(1) {
        if((port->command_issue & (1 << slot)) == 0) {
            break;
        }

        if(port->interrupt_status & AHCI_HBA_PxIS_TFES) {
            PRINTLOG(AHCI, LOG_FATAL, "disk identify error");
            bit_clear32(&disk->acquired_slots, slot);

            return -1;
        }
    }

    if(port->interrupt_status & AHCI_HBA_PxIS_TFES) {
        PRINTLOG(AHCI, LOG_FATAL, "disk identify error");
        bit_clear32(&disk->acquired_slots, slot);

        return -1;
    }

    bit_clear32(&disk->acquired_slots, slot);

    PRINTLOG(AHCI, LOG_TRACE, "building identify data");

    disk->cylinders = identify_data.num_cylinders;
    disk->heads = identify_data.num_heads;
    disk->sectors = identify_data.num_sectors_per_track;

    if(identify_data.command_set_support.big_lba) {
        disk->lba_count = identify_data.user_addressable_sectors_ext;
    } else {
        disk->lba_count = identify_data.user_addressable_sectors;
    }

    for(uint8_t i = 0; i < 20; i += 2) {
        disk->serial[i] = identify_data.serial_number[i + 1];
        disk->serial[i + 1] = identify_data.serial_number[i];
    }

    for(uint8_t i = 20; i > 0; i--) {
        if(disk->serial[i - 1] != 0x20) {
            break;
        }
        disk->serial[i - 1] = NULL;
    }

    for(uint8_t i = 0; i < 40; i += 2) {
        disk->model[i] = identify_data.model_number[i + 1];
        disk->model[i + 1] = identify_data.model_number[i];
    }

    for(uint8_t i = 40; i > 0; i--) {
        if(disk->model[i - 1] != 0x20) {
            break;
        }
        disk->model[i - 1] = NULL;
    }

    disk->queue_depth = identify_data.queue_depth + 1;
    disk->sncq &= identify_data.serial_ata_capabilities.ncq;
    disk->volatile_write_cache = identify_data.command_set_support.write_cache | identify_data.command_set_active.write_cache;

    disk->logging.fields.gpl_supported = identify_data.command_set_support.gp_logging;
    disk->logging.fields.gpl_enabled = identify_data.command_set_active.gp_logging;
    disk->logging.fields.dma_ext_supported = identify_data.command_set_support_ext.read_write_log_dma_ext;
    disk->logging.fields.dma_ext_enabled = identify_data.command_set_active_ext.read_write_log_dma_ext;
    disk->logging.fields.dma_ext_is_log_ext = identify_data.serial_ata_capabilities.read_logdma;

    disk->smart_status.fields.supported = identify_data.command_set_support.smart_commands;
    disk->smart_status.fields.enabled = identify_data.command_set_active.smart_commands;
    disk->smart_status.fields.errlog_supported = identify_data.command_set_support.smart_error_log;
    disk->smart_status.fields.errlog_enabled = identify_data.command_set_active.smart_error_log;
    disk->smart_status.fields.selftest_supported = identify_data.command_set_support.smart_self_test;
    disk->smart_status.fields.selftest_enabled = identify_data.command_set_active.smart_self_test;

    if(identify_data.physical_logical_sector_size.logical_sector_longer_than256words) {
        disk->logical_sector_size = 4096;
    } else {
        disk->logical_sector_size = 512;
    }

    disk->physical_sector_size = disk->logical_sector_size << identify_data.physical_logical_sector_size.logical_sectors_per_physical_sector;

    PRINTLOG(AHCI, LOG_INFO, "disk %lli cyl %x head %x sec %x lba %llx serial %s model %s queue depth %i sncq %i vwc %i logging %i smart %x physical sector size %llx logical sector size %llx",
             disk->disk_id, disk->cylinders, disk->heads,
             disk->sectors, disk->lba_count, disk->serial, disk->model,
             disk->queue_depth, disk->sncq, disk->volatile_write_cache,
             disk->logging.bits, disk->smart_status.bits,
             disk->physical_sector_size, disk->logical_sector_size);

    port->sata_active = 0;
    port->interrupt_status = (uint32_t)-1;
    port->interrupt_enable = (uint32_t)-1;

    return 0;
}

future_t* ahci_read(uint64_t disk_id, uint64_t lba, uint32_t size, uint8_t* buffer) {
    ahci_sata_disk_t* disk = (ahci_sata_disk_t*)list_get_data_at_position(sata_ports, disk_id);

    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    int32_t slot = ahci_find_command_slot(disk);

    if(slot == -1) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot find empty command slot");
        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "read port 0x%p slot %i", port, slot);
    PRINTLOG(AHCI, LOG_TRACE, "read disk 0x%p lba 0x%llx size 0x%x buffer 0x%p", disk, lba, size, buffer);
    PRINTLOG(AHCI, LOG_TRACE, "logical sector size 0x%llx", disk->logical_sector_size);

    uint32_t sector_count = size / disk->logical_sector_size;

    if(size % disk->logical_sector_size != 0) {
        sector_count++;
    }

    if(sector_count > 65536) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot read more than 65536 sectors at on");

        return NULL;
    }

    uint32_t prdt_length = size / (4 << 20);

    if(size % (4 << 20) != 0) {
        prdt_length++;
    }

    if(prdt_length > 64) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot read more than 0x%llx at on", 65536 * disk->logical_sector_size);

        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "size 0x%x sector count 0x%x prdt length 0x%x", size, sector_count, prdt_length);

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address);
    cmd_hdr += slot;

    cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->write_direction = 0;
    cmd_hdr->prdt_length = prdt_length;
    cmd_hdr->clear_busy = 1;

    ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_hdr->prdt_base_address);
    memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

    uint64_t buffer_phy_addr = 0;

    if(memory_paging_get_physical_address((uint64_t)buffer, &buffer_phy_addr) != 0) {
        PRINTLOG(AHCI, LOG_ERROR, "cannot get read buffer physical address 0x%p", buffer);

        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "buffer 0x%p physical address 0x%llx", buffer, buffer_phy_addr);


    uint32_t tmp_size = size;

    for(uint32_t i = 0; i < cmd_hdr->prdt_length; i++) {
        cmd_table->prdt_entry[i].data_base_address = buffer_phy_addr;
        cmd_table->prdt_entry[i].data_byte_count = (tmp_size > (4 << 20)) ? (4 << 20) - 1 : tmp_size - 1;

        PRINTLOG(AHCI, LOG_TRACE, "prdt entry %i data base address 0x%llx data byte count 0x%x", i, cmd_table->prdt_entry[i].data_base_address, cmd_table->prdt_entry[i].data_byte_count);

        tmp_size -= (4 << 20);
        buffer_phy_addr += (4 << 20);
    }

    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(&cmd_table->command_fis);
    memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->control_or_command = 1;

    if(!(disk->sncq && disk->queue_depth)) {
        fis->command = AHCI_ATA_CMD_READ_DMA_EXT;
        fis->count = size / disk->logical_sector_size;
    }else {
        fis->command = AHCI_ATA_CMD_READ_FPDMA_QUEUED;
        fis->count = slot << 3;
        fis->featurel = (size / disk->logical_sector_size) & 0xFF;
        fis->featureh = ((size / disk->logical_sector_size) >> 8) & 0xFF;

        PRINTLOG(AHCI, LOG_TRACE, "read fpdma queued count 0x%x featurel 0x%x featureh 0x%x", fis->count, fis->featurel, fis->featureh);
    }

    fis->lba0 = lba & 0xFFFFFF;
    fis->lba1 = (lba >> 24) & 0xFFFFFF;
    fis->device = 1 << 6;

    PRINTLOG(AHCI, LOG_TRACE, "read lba 0x%llx lba0 0x%x lba1 0x%x", lba, fis->lba0, fis->lba1);

    uint64_t spin = 0;

    while((port->task_file_data  & (AHCI_ATA_DEV_BUSY | AHCI_ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }

    if(spin == 1000000) {
        PRINTLOG(AHCI, LOG_FATAL, "disk is busy");
        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "disk is not busy spin: 0x%llx", spin);

    if(disk->sncq && disk->queue_depth) {
        port->sata_active = 1 << slot;
    }

    uint64_t tid = task_get_id();

    disk->future_locks[slot] = lock_create_with_heap_for_future(disk->heap, true, tid);

    future_t* fut = future_create_with_heap_and_data(disk->heap, disk->future_locks[slot], buffer);


    bit_set32(&disk->current_commands, slot);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    bit_set32(&port->command_issue, slot);
#pragma GCC diagnostic pop

    return fut;
}

future_t* ahci_write(uint64_t disk_id, uint64_t lba, uint32_t size, uint8_t* buffer) {
    ahci_sata_disk_t* disk = (ahci_sata_disk_t*)list_get_data_at_position(sata_ports, disk_id);

    ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    int32_t slot = ahci_find_command_slot(disk);

    if(slot == -1) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot find empty command slot");
        return NULL;
    }

    uint32_t sector_count = size / disk->logical_sector_size;

    if(size % disk->logical_sector_size != 0) {
        sector_count++;
    }

    if(sector_count > 65536) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot write more than 65536 sectors at on");

        return NULL;
    }

    uint32_t prdt_length = size / (4 << 20);

    if(size % (4 << 20) != 0) {
        prdt_length++;
    }

    if(prdt_length > 64) {
        PRINTLOG(AHCI, LOG_FATAL, "cannot write more than 0x%llx at on", 65536 * disk->logical_sector_size);

        return NULL;
    }

    PRINTLOG(AHCI, LOG_TRACE, "write to port 0x%p at lba 0x%llx with size 0x%x from buffer 0x%p slot %i", port, lba, size, buffer, slot);

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address);
    cmd_hdr += slot;

    cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->write_direction = 1;
    cmd_hdr->prdt_length = prdt_length;
    cmd_hdr->clear_busy = 1;

    ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_hdr->prdt_base_address);
    memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

    uint64_t buffer_phy_addr = 0;

    if(memory_paging_get_physical_address((uint64_t)buffer, &buffer_phy_addr) != 0) {
        PRINTLOG(AHCI, LOG_ERROR, "cannot get read buffer physical address 0x%p", buffer);

        return NULL;
    }

    uint32_t tmp_size = size;

    for(uint32_t i = 0; i < cmd_hdr->prdt_length; i++) {
        cmd_table->prdt_entry[i].data_base_address = buffer_phy_addr;
        cmd_table->prdt_entry[i].data_byte_count = (tmp_size > (4 << 20)) ? (4 << 20) - 1 : tmp_size - 1;

        tmp_size -= (4 << 20);
        buffer_phy_addr += (4 << 20);
    }

    ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(&cmd_table->command_fis);
    memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

    fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
    fis->control_or_command = 1;

    if(!(disk->sncq && disk->queue_depth)) {
        fis->command = AHCI_ATA_CMD_WRITE_DMA_EXT;
        fis->count = size / disk->logical_sector_size;
    }else {
        fis->command = AHCI_ATA_CMD_WRITE_FPDMA_QUEUED;
        fis->count = slot << 3;
        fis->featurel = (size / disk->logical_sector_size) & 0xFF;
        fis->featureh = ((size / disk->logical_sector_size) >> 8) & 0xFF;
        fis->device = 3 << 6; // fua and always 1
    }

    fis->lba0 = lba & 0xFFFFFF;
    fis->lba1 = (lba >> 24) & 0xFFFFFF;

    uint64_t spin = 0;

    while((port->task_file_data  & (AHCI_ATA_DEV_BUSY | AHCI_ATA_DEV_DRQ)) && spin < 1000000) {
        spin++;
    }

    if(spin == 1000000) {
        PRINTLOG(AHCI, LOG_FATAL, "disk is busy");
        return NULL;
    }

    if(disk->sncq && disk->queue_depth) {
        port->sata_active = 1 << slot;
    }

    uint64_t tid = task_get_id();

    disk->future_locks[slot] = lock_create_with_heap_for_future(disk->heap, true, tid);

    future_t* fut = future_create_with_heap_and_data(disk->heap, disk->future_locks[slot], buffer);

    bit_set32(&disk->current_commands, slot);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    bit_set32(&port->command_issue, slot);
#pragma GCC diagnostic pop

    return fut;
}


ahci_device_type_t ahci_check_type(ahci_hba_port_t* port) {
    uint32_t sata_status = port->sata_status;

    PRINTLOG(AHCI, LOG_TRACE, "port status 0x%x", sata_status);

    uint8_t ipm = (sata_status >> 8) & 0x0F;
    uint8_t det = sata_status & 0x0F;

    if (det != AHCI_HBA_PORT_DET_PRESENT) {
        return AHCI_DEVICE_NULL;
    }

    if (ipm != AHCI_HBA_PORT_IPM_ACTIVE) {
        return AHCI_DEVICE_NULL;
    }

    switch (port->signature)
    {
    case  AHCI_SATA_SIG_ATAPI:
        return AHCI_DEVICE_SATAPI;
    case AHCI_SATA_SIG_SEMB:
        return AHCI_DEVICE_SEMB;
    case AHCI_SATA_SIG_PM:
        return AHCI_DEVICE_PM;
    default:
        return AHCI_DEVICE_SATA;
    }
}


int8_t ahci_find_command_slot(ahci_sata_disk_t* disk) {
    // ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

    lock_acquire(disk->disk_lock);

    for(int8_t i = 0; i < disk->command_count; i++) {
        if(bit_test32(&disk->acquired_slots, i) == 0) {
            bit_set32(&disk->acquired_slots, i);

            lock_release(disk->disk_lock);

            return i;
        }
    }

    lock_release(disk->disk_lock);

    PRINTLOG(AHCI, LOG_WARNING, "cannot find empty command slot");

    return -1;
}

void ahci_port_start_cmd(ahci_hba_port_t* port) {
    PRINTLOG(AHCI, LOG_TRACE, "try to start port 0x%p", port);

    PRINTLOG(AHCI, LOG_TRACE, "sending identify 0x%x 0x%x 0x%x", port->interrupt_status, port->task_file_data, port->sata_error);
    // Wait until CR (bit15) is cleared
    while (port->command_and_status & AHCI_HBA_PxCMD_CR);  // check command running

    // Set FRE (bit4) and ST (bit0)
    port->command_and_status |= AHCI_HBA_PxCMD_FRE; // set fis receive enable
    port->command_and_status |= AHCI_HBA_PxCMD_ST; // set command start

    // port->sata_control = 1;
    PRINTLOG(AHCI, LOG_TRACE, "sending identify 0x%x 0x%x 0x%x", port->interrupt_status, port->task_file_data, port->sata_error);

    PRINTLOG(AHCI, LOG_TRACE, "port 0x%p started", port);
}

// Stop command engine
void ahci_port_stop_cmd(ahci_hba_port_t* port) {
    PRINTLOG(AHCI, LOG_TRACE, "try to stop port 0x%p", port);
    // Clear ST (bit0)
    port->command_and_status &= ~AHCI_HBA_PxCMD_ST;

    // Clear FRE (bit4)
    port->command_and_status &= ~AHCI_HBA_PxCMD_FRE;

    // Wait until FR (bit14), CR (bit15) are cleared
    while(1) {
        if (port->command_and_status & AHCI_HBA_PxCMD_FR) { // loop while fis receiving
            continue;
        }

        if (port->command_and_status & AHCI_HBA_PxCMD_CR) { // loop while command running
            continue;
        }

        break;
    }

    PRINTLOG(AHCI, LOG_TRACE, "port 0x%p stopped", port);
}

void ahci_port_rebase(ahci_hba_port_t* port, uint64_t offset, int8_t nr_cmd_slots) {
    ahci_port_stop_cmd(port);

    PRINTLOG(AHCI, LOG_TRACE, "port 0x%p is rebasing to 0x%llx", port, offset);

    port->command_list_base_address = offset;
    memory_memclean((void*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address), 1024);

    offset += 1024;

    port->fis_base_address = offset;
    memory_memclean((void*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->fis_base_address), 256);

    offset += 256;

    ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(port->command_list_base_address);

    for(uint8_t i = 0; i < nr_cmd_slots; i++) {
        cmd_hdr[i].prdt_length = 64;
        cmd_hdr[i].prdt_base_address = offset;

        uint64_t size = sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr[i].prdt_length - 1));

        PRINTLOG(AHCI, LOG_TRACE, "prdt offset 0x%llx size 0x%llx", offset, size);

        memory_memclean((void*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(cmd_hdr[i].prdt_base_address), size);

        offset += size;
    }

    ahci_port_start_cmd(port);
}
