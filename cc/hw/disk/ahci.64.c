#include <driver/ahci.h>
#include <video.h>
#include <pci.h>
#include <memory/paging.h>
#include <cpu/interrupt.h>
#include <apic.h>
#include <cpu.h>
#include <time/timer.h>

linkedlist_t sata_ports = NULL;
linkedlist_t sata_hbas = NULL;

ahci_device_type_t ahci_check_type(ahci_hba_port_t* port);
int8_t ahci_find_command_slot(ahci_hba_port_t* port, int8_t nr_cmd_slots);
void ahci_port_rebase(ahci_hba_port_t* port, uint64_t offset, int8_t nr_cmd_slots);
void ahci_port_start_cmd(ahci_hba_port_t* port);
void ahci_port_stop_cmd(ahci_hba_port_t* port);
void ahci_handle_disk_isr(ahci_hba_t* hba, uint64_t disk_id);
int8_t ahci_error_recovery_ncq(ahci_hba_port_t* port, ahci_sata_disk_t* disk);
int8_t ahci_read_log_ncq(ahci_hba_port_t* port, uint8_t from_dma);

int8_t ahci_isr(interrupt_frame_t* frame, uint8_t intnum);

int8_t ahci_isr(interrupt_frame_t* frame, uint8_t intnum){
	UNUSED(frame);

	boolean_t irq_handled = 0;

	iterator_t* iter = linkedlist_iterator_create(sata_hbas);

	while(iter->end_of_iterator(iter) != 0) {
		ahci_hba_t* hba = iter->get_item(iter);

		if(hba->intnum_base <= intnum && intnum < (hba->intnum_base + hba->intnum_count)) {
			ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)hba->hba_addr;

			if(hba->intnum_count == 1) {
				uint32_t is = hba_mem->interrupt_status;

				for(uint8_t i = 0; i < hba->disk_count; i++) {
					if(is & 1) {
						ahci_handle_disk_isr(hba, hba->disk_base + i);
						irq_handled = 1;
					}

					is >>= 1;
				}

			} else {
				ahci_handle_disk_isr(hba, hba->disk_base + intnum - hba->intnum_base);
				irq_handled = 1;
			}

			break;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	if(irq_handled) {
		apic_eoi();
		cpu_sti();

		return 0;
	}

	return -1;
}

void ahci_handle_disk_isr(ahci_hba_t* hba, uint64_t disk_id) {
	ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)hba->hba_addr;
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);
	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	printf("AHCI: Info interrupt status 0x%x cmd_status 0x%x sactive 0x%x serror 0x%x cmdissued 0x%x tfd 0x%x\n",
	       port->interrupt_status, port->command_and_status, port->sata_active, port->sata_error, port->command_issue, port->task_file_data);

	if(port->task_file_data & 1) {
		if(port->sata_active) {
			ahci_error_recovery_ncq(port, disk);
		}
	}

	port->interrupt_status = (uint32_t)-1;
	hba_mem->interrupt_status = 1;
}

int8_t ahci_error_recovery_ncq(ahci_hba_port_t* port, ahci_sata_disk_t* disk){
	port->command_and_status &= ~AHCI_HBA_PxCMD_ST;

	while(1) {
		if (port->command_and_status & AHCI_HBA_PxCMD_CR) { // loop while command running
			continue;
		}

		break;
	}

	port->sata_error = (uint32_t)-1;
	port->interrupt_status = (uint32_t)-1;

	port->command_and_status |= AHCI_HBA_PxCMD_ST;   // set command start

	if(disk->logging.gpl_enabled) {
		port->interrupt_enable = 0;
		port->interrupt_status = (uint32_t)-1;

		ahci_read_log_ncq(port, disk->logging.dma_ext_is_log_ext);

		port->interrupt_status = (uint32_t)-1;
		port->interrupt_enable = (uint32_t)-1;
	}

	if(port->sata_active) {
		printf("AHCI: Warning force comreset\n");
		port->sata_control |= 1;
		time_timer_spinsleep(1000);
		port->sata_control &= ~1;
		while((port->sata_status & 0xF) != 0x3);
		port->sata_error = (uint32_t)-1;
	}

	return 0;
}

int8_t ahci_read_log_ncq(ahci_hba_port_t* port, uint8_t from_dma) {

	ahci_ata_ncq_error_log_t error_log;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	printf("slot %i\n", slot);

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 0;
	cmd_hdr->prdt_length = 1;
	cmd_hdr->clear_busy = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)&error_log;
	cmd_table->prdt_entry[0].data_byte_count = 511;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
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
			printf("AHCI: Fatal read ncq log error is 0x%x tfd 0x%x err 0x%x\n", port->interrupt_status, port->task_file_data, port->sata_error);
			return -1;
		}
	}

	if(port->interrupt_status & 1) {
		ahci_hba_fis_t* hba_fis = (ahci_hba_fis_t*)port->fis_base_address;
		printf("AHCI: hba fis lba %lx count %x err %x status %x\n",
		       hba_fis->d2h_fis.lba0 | ( hba_fis->d2h_fis.lba1 << 24), hba_fis->d2h_fis.count, hba_fis->d2h_fis.error, hba_fis->d2h_fis.status );
	}

	printf("AHCI: Error is 0x%x 0x%x 0x%x ncq 0x%x unload 0x%x tag 0x%x status 0x%x error 0x%x lba 0x%lx count 0x%x device 0x%x \n",
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

int8_t ahci_init(memory_heap_t* heap, linkedlist_t sata_pci_devices, uint64_t ahci_offset) {
	if(linkedlist_size(sata_pci_devices) == 0) {
		printf("AHCI: Warning no SATA devices\n");
		return 0;
	}

	uint64_t disk_id = 0;

	sata_ports = linkedlist_create_sortedlist_with_heap(heap, ahci_disk_id_comparator);
	sata_hbas = linkedlist_create_list_with_heap(heap);

	iterator_t* iter = linkedlist_iterator_create(sata_pci_devices);

	while(iter->end_of_iterator(iter) != 0) {
		pci_dev_t* p = iter->get_item(iter);
		pci_generic_device_t* pci_sata = (pci_generic_device_t*)p->pci_header;


		pci_capability_msi_t* msi_cap = NULL;

		if(pci_sata->common_header.status.capabilities_list) {
			pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pci_sata) + pci_sata->capabilities_pointer);


			while(pci_cap->capability_id != 0xFF) {
				if(pci_cap->capability_id == PCI_DEVICE_CAPABILITY_MSI) {
					msi_cap = (pci_capability_msi_t*)pci_cap;
					break;
				}

				if(pci_cap->next_pointer == NULL) {
					break;
				}

				pci_cap = (pci_capability_t*)(((uint8_t*)pci_cap ) + pci_cap->next_pointer);
			}
		}

		uint32_t abar = (uint32_t)(pci_sata->bar5.memory_space_bar.base_address << 4);

		printf("AHCI: abar value 0x%x bar4  0x%x\n", abar, pci_sata->bar4.io_space_bar.base_address << 2);

		memory_paging_add_page_ext(heap, NULL, abar, abar, MEMORY_PAGING_PAGE_TYPE_4K);
		memory_paging_add_page_ext(heap, NULL, abar + MEMORY_PAGING_PAGE_LENGTH_4K, abar + MEMORY_PAGING_PAGE_LENGTH_4K, MEMORY_PAGING_PAGE_TYPE_4K);

		ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)((uint64_t)abar);

		uint8_t nr_cmd_slots = ( (hba_mem->host_capability >> 8) & 0x1F) + 1;
		uint8_t nr_port = (hba_mem->host_capability & 0x1F) + 1;
		uint8_t sncq  = (hba_mem->host_capability >> 30) & 1;

		ahci_hba_t* hba = memory_malloc_ext(heap, sizeof(ahci_hba_t), 0x0);
		hba->hba_addr = (uint64_t)abar;
		hba->disk_base = disk_id;
		hba->disk_count = nr_port;

		linkedlist_list_insert(sata_hbas, hba);

		if(msi_cap) {
			printf("AHCI: msi capability present\n");

			uint8_t msg_count = 1 << msi_cap->multiple_message_count;

			printf("AHCI: ma64 support %i vector masking %i msg count %i\n", msi_cap->ma64_support, msi_cap->per_vector_masking, msg_count);

			uint8_t intnum = interrupt_get_next_empty_interrupt();
			hba->intnum_base = intnum - INTERRUPT_IRQ_BASE;
			hba->intnum_count = msg_count;

			if(msi_cap->ma64_support) {
				msi_cap->ma64.message_address = 0xFEE00000 | (1 << 3) | (0 << 2);
				msi_cap->ma64.message_data = intnum;
			} else {
				msi_cap->ma32.message_address = 0xFEE00000 | (1 << 3) | (0 << 2);
				msi_cap->ma32.message_data = intnum;
			}

			for(uint8_t i = 0; i < msg_count; i++) {
				uint8_t isrnum = intnum - INTERRUPT_IRQ_BASE;
				interrupt_irq_set_handler(isrnum, &ahci_isr);
				printf("AHCI: Info interrupt 0x%x registered as isr 0x%x for ahci_isr\n", intnum, isrnum);
				intnum = interrupt_get_next_empty_interrupt();
			}

			msi_cap->enable = 1;

		} else {
			printf("AHCI: no msi capability\n");

			return -1;
		}

		hba_mem->global_host_control = 1;
		hba_mem->global_host_control = (1 << 31);

		printf("AHCI: controller port cnt %i cmd slots %i sncq %i bohc %x\n", nr_port, nr_cmd_slots, sncq, hba_mem->bios_os_handoff_control_and_status);

		for(uint8_t port_idx = 0; port_idx < nr_port; port_idx++) {
			printf("AHCI: checking port %i at ahci bar 0x%lx", port_idx, abar);

			uint64_t port_address = (uint64_t)&hba_mem->ports[port_idx];

			printf(" port address 0x%lx\n", port_address);

			ahci_device_type_t dt = ahci_check_type(&hba_mem->ports[port_idx]);

			ahci_sata_disk_t* disk = memory_malloc_ext(heap, sizeof(ahci_sata_disk_t), 0x0);
			disk->disk_id = disk_id++;
			disk->port_address = port_address;
			disk->type = dt;
			disk->sncq = sncq;

			linkedlist_list_insert(sata_ports, disk);

			if (dt == AHCI_DEVICE_SATA) {
				printf("AHCI: SATA drive found at port %d at 0x%lx\n", port_idx, port_address);

				for(int8_t i = 0; i < 4; i++) {
					memory_paging_add_page_ext(heap, NULL, ahci_offset, ahci_offset, MEMORY_PAGING_PAGE_TYPE_4K);
					ahci_offset += MEMORY_PAGING_PAGE_LENGTH_4K;
				}

				ahci_port_rebase( &hba_mem->ports[port_idx], ahci_offset - 4 * MEMORY_PAGING_PAGE_LENGTH_4K, nr_cmd_slots);

				if(ahci_identify(disk->disk_id) != 0) {
					printf("AHCI: Error cannot identify disk %li at port address %lx\n", disk_id, port_address);
				}

			}   else if (dt == AHCI_DEVICE_SATAPI)  {
				printf("AHCI: Debug SATAPI drive found at port %d\n", port_idx);
			}   else if (dt == AHCI_DEVICE_SEMB)  {
				printf("AHCI: Debug SEMB drive found at port %d\n", port_idx);
			}   else if (dt == AHCI_DEVICE_PM) {
				printf("AHCI: Debug PM drive found at port %d\n", port_idx);
			}   else {
				printf("AHCI: Debug No drive found at port %d\n", port_idx);
			}

		}

		hba_mem->global_host_control = (1 << 1); // enable interrupts

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	return linkedlist_size(sata_ports);
}


int8_t ahci_flush(uint64_t disk_id) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	printf("AHCI: Info flush port 0x%p slot %i\n", port, slot);

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 0;
	cmd_hdr->prdt_length = 0;
	cmd_hdr->clear_busy = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t));

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;
	fis->command = AHCI_ATA_CMD_FLUSH_EXT;

	port->command_issue = 1 << slot;

	return 0;
}

int8_t ahci_identify(uint64_t disk_id) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	ahci_ata_identify_data_t identify_data;

	port->interrupt_status = (uint32_t)-1;
	port->sata_error = (uint32_t)-1;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 0;
	cmd_hdr->prdt_length = 1;
	cmd_hdr->clear_busy = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)&identify_data;
	cmd_table->prdt_entry[0].data_byte_count = 511;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
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
			printf("AHCI: Fatal disk identify error\n");
			return -1;
		}
	}

	if(port->interrupt_status & AHCI_HBA_PxIS_TFES) {
		printf("AHCI: Fatal disk identify error\n");
		return -1;
	}

	printf("AHCI: Info building identify data\n");

	disk->cylinders = identify_data.cylinders;
	disk->heads = identify_data.heads;
	disk->sectors = identify_data.sectors_per_track;

	if(1 & (identify_data.command_set_supported_83 >> 10)) {
		disk->lba_count = identify_data.user_addressable_sectors_ext;
	} else {
		disk->lba_count = identify_data.user_addressable_sectors;
	}

	for(uint8_t i = 0; i < 20; i += 2) {
		disk->serial[i] = identify_data.serial_no[i + 1];
		disk->serial[i + 1] = identify_data.serial_no[i];
	}

	for(uint8_t i = 20; i > 0; i--) {
		if(disk->serial[i - 1] != 0x20) {
			break;
		}
		disk->serial[i - 1] = NULL;
	}

	for(uint8_t i = 0; i < 40; i += 2) {
		disk->model[i] = identify_data.model_name[i + 1];
		disk->model[i + 1] = identify_data.model_name[i];
	}

	for(uint8_t i = 40; i > 0; i--) {
		if(disk->model[i - 1] != 0x20) {
			break;
		}
		disk->model[i - 1] = NULL;
	}

	disk->queue_depth = identify_data.queue_depth + 1;
	disk->sncq &= (identify_data.serial_ata_capabilities >> 8) & 1;
	disk->volatile_write_cache = ((identify_data.command_set_supported_82 >> 5) & 1 ) | (((identify_data.command_set_feature_enabled_85 >> 5) & 1) << 1);

	disk->logging.gpl_supported = (identify_data.command_set_supported_84 >> 5) & 1;
	disk->logging.gpl_enabled = (identify_data.command_set_feature_enabled_87 >> 5) & 1;
	disk->logging.dma_ext_supported = (identify_data.command_set_supported_119 >> 3) & 1;
	disk->logging.dma_ext_enabled = (identify_data.command_set_feature_enabled_120 >> 3) & 1;
	disk->logging.dma_ext_is_log_ext = (identify_data.serial_ata_capabilities >> 15) & 1;

	disk->smart_status.supported = identify_data.command_set_supported_82 & 1;
	disk->smart_status.enabled = identify_data.command_set_feature_enabled_85 & 1;
	disk->smart_status.errlog_supported = identify_data.command_set_supported_84 & 1;
	disk->smart_status.errlog_enabled = identify_data.command_set_feature_enabled_87 & 1;
	disk->smart_status.selftest_supported = (identify_data.command_set_supported_84 >> 1) & 1;
	disk->smart_status.selftest_enabled = (identify_data.command_set_feature_enabled_87 >> 1) & 1;

	printf("AHCI: Info disk %li cyl %x head %x sec %x lba %lx serial %s model %s queue depth %i sncq %i vwc %i logging %i smart %x\n",
	       disk->disk_id, disk->cylinders, disk->heads,
	       disk->sectors, disk->lba_count, disk->serial, disk->model,
	       disk->queue_depth, disk->sncq, disk->volatile_write_cache,
	       disk->logging, disk->smart_status);

	port->sata_active = 0;
	port->interrupt_status = (uint32_t)-1;
	port->interrupt_enable = (uint32_t)-1;

	return 0;
}

int8_t ahci_read(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 0;
	cmd_hdr->prdt_length = 1;
	cmd_hdr->clear_busy = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)buffer;
	cmd_table->prdt_entry[0].data_byte_count = size - 1;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;

	if(!(disk->sncq && disk->queue_depth)) {
		fis->command = AHCI_ATA_CMD_READ_DMA_EXT;
		fis->count = size / 512;
	}else {
		fis->command = AHCI_ATA_CMD_READ_FPDMA_QUEUED;
		fis->count = slot << 3;
		fis->featurel = (size / 512) & 0xFF;
		fis->featureh = ((size / 512) >> 8) & 0xFF;
	}

	fis->lba0 = lba & 0xFFFFFF;
	fis->lba1 = (lba >> 24) & 0xFFFFFF;
	fis->device = 1 << 6;

	if(disk->sncq && disk->queue_depth) {
		port->sata_active = 1 << slot;
	}

	port->command_issue = 1 << slot;

	return 0;
}

int8_t ahci_write(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	printf("AHCI: Info write to port 0x%p at lba 0x%lx with size 0x%x from buffer 0x%p slot %i\n", port, lba, size, buffer, slot);

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 1;
	cmd_hdr->prdt_length = 1;
	cmd_hdr->clear_busy = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)buffer;
	cmd_table->prdt_entry[0].data_byte_count = size - 1;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;

	if(!(disk->sncq && disk->queue_depth)) {
		fis->command = AHCI_ATA_CMD_WRITE_DMA_EXT;
		fis->count = size / 512;
	}else {
		fis->command = AHCI_ATA_CMD_WRITE_FPDMA_QUEUED;
		fis->count = slot << 3;
		fis->featurel = (size / 512) & 0xFF;
		fis->featureh = ((size / 512) >> 8) & 0xFF;
		fis->device = 3 << 6; // fua and always 1
	}

	fis->lba0 = lba & 0xFFFFFF;
	fis->lba1 = (lba >> 24) & 0xFFFFFF;

	if(disk->sncq && disk->queue_depth) {
		port->sata_active = 1 << slot;
	}

	port->command_issue = 1 << slot;

	return 0;
}


ahci_device_type_t ahci_check_type(ahci_hba_port_t* port) {
	uint32_t sata_status = port->sata_status;

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


int8_t ahci_find_command_slot(ahci_hba_port_t* port, int8_t nr_cmd_slots) {
	uint32_t slots = port->sata_active | port->command_issue;

	for(int8_t i = 0; i < nr_cmd_slots; i++) {
		if((slots & 1) == 0) {
			return i;
		}

		slots >>= 1;
	}
	printf("AHCI: Warning cannot find empty command slot\n");
	return -1;
}

void ahci_port_start_cmd(ahci_hba_port_t* port) {
	printf("AHCI: Info try to start port 0x%p\n", port);

	printf("AHCI: Info sending identify 0x%x 0x%x 0x%x\n", port->interrupt_status, port->task_file_data, port->sata_error);
	// Wait until CR (bit15) is cleared
	while (port->command_and_status & AHCI_HBA_PxCMD_CR); // check command running

	// Set FRE (bit4) and ST (bit0)
	port->command_and_status |= AHCI_HBA_PxCMD_FRE;  // set fis receive enable
	port->command_and_status |= AHCI_HBA_PxCMD_ST;   // set command start

	//port->sata_control = 1;
	printf("AHCI: Info sending identify 0x%x 0x%x 0x%x\n", port->interrupt_status, port->task_file_data, port->sata_error);

	printf("AHCI: Info port 0x%p started\n", port);
}

// Stop command engine
void ahci_port_stop_cmd(ahci_hba_port_t* port) {
	printf("AHCI: Info try to stop port 0x%p\n", port);
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

	printf("AHCI: Info port 0x%p stopped\n", port);
}

void ahci_port_rebase(ahci_hba_port_t* port, uint64_t offset, int8_t nr_cmd_slots) {
	ahci_port_stop_cmd(port);

	printf("AHCI: Info port 0x%p is rebasing to 0x%lx\n", port, offset);

	port->command_list_base_address = offset;
	memory_memclean((void*)port->command_list_base_address, 1024);

	offset += 1024;

	port->fis_base_address = offset;
	memory_memclean((void*)port->fis_base_address, 256);

	offset += 256;

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;

	for(uint8_t i = 0; i < nr_cmd_slots; i++) {
		cmd_hdr[i].prdt_length = 16;
		cmd_hdr[i].prdt_base_address = offset;

		uint64_t size = sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr[i].prdt_length - 1));

		memory_memclean((void*)cmd_hdr[i].prdt_base_address, size);

		offset += size;
	}

	ahci_port_start_cmd(port);
}
