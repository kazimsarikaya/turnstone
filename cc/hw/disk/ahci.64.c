#include <driver/ahci.h>
#include <video.h>
#include <pci.h>
#include <memory/paging.h>

linkedlist_t sata_ports = NULL;

ahci_device_type_t ahci_check_type(ahci_hba_port_t* port);
int8_t ahci_find_command_slot(ahci_hba_port_t* port, int8_t nr_cmd_slots);
void ahci_port_rebase(ahci_hba_port_t* port, uint64_t offset, int8_t nr_cmd_slots);
void ahci_port_start_cmd(ahci_hba_port_t* port);
void ahci_port_stop_cmd(ahci_hba_port_t* port);

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

	sata_ports = linkedlist_create_sortedlist_with_heap(heap, ahci_disk_id_comparator);

	iterator_t* iter = linkedlist_iterator_create(sata_pci_devices);

	while(iter->end_of_iterator(iter) != 0) {
		pci_dev_t* p = iter->get_item(iter);
		pci_generic_device_t* pci_sata = (pci_generic_device_t*)p->pci_header;

		uint32_t abar = (uint32_t)(pci_sata->bar5.memory_space_bar.base_address << 4);

		printf("AHCI: abar value 0x%x\n", abar);

		memory_paging_add_page_ext(heap, NULL, abar, abar, MEMORY_PAGING_PAGE_TYPE_4K);
		memory_paging_add_page_ext(heap, NULL, abar + MEMORY_PAGING_PAGE_LENGTH_4K, abar + MEMORY_PAGING_PAGE_LENGTH_4K, MEMORY_PAGING_PAGE_TYPE_4K);

		ahci_hba_mem_t* hba_mem = (ahci_hba_mem_t*)((uint64_t)abar);

		uint8_t nr_cmd_slots = ( (hba_mem->host_capability >> 8) & 0x1F) + 1;
		uint8_t nr_port = (hba_mem->host_capability & 0x1F) + 1;

		hba_mem->global_host_control = (1 << 31);
		hba_mem->global_host_control = 1;

		printf("AHCI: controller port cnt %i cmd slots %i\n", nr_port, nr_cmd_slots);

		uint32_t pi = hba_mem->port_implemented;

		uint64_t disk_id = 0;

		for(uint8_t port_idx = 0; port_idx < nr_port; port_idx++) {
			if (pi & 1)   {

				printf("AHCI: checking port %i at ahci bar 0x%lx", port_idx, abar);

				uint64_t port_address = (uint64_t)&hba_mem->ports[port_idx];

				printf(" port address 0x%lx\n", port_address);

				ahci_device_type_t dt = ahci_check_type(&hba_mem->ports[port_idx]);

				if (dt == AHCI_DEVICE_SATA) {
					printf("AHCI: SATA drive found at port %d at 0x%lx\n", port_idx, port_address);

					ahci_sata_disk_t* disk = memory_malloc_ext(heap, sizeof(ahci_sata_disk_t), 0x0);
					disk->disk_id = disk_id++;
					disk->port_address = port_address;

					linkedlist_list_insert(sata_ports, disk);

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

			pi >>= 1;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	return linkedlist_size(sata_ports);
}


int8_t ahci_flush(uint64_t disk_id) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	port->interrupt_state = (uint32_t)-1;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 0;
	cmd_hdr->prdt_length = 0;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t));

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;
	fis->command = AHCI_ATA_CMD_FLUSH_EX;

	uint32_t spin = 1000000;

	while((port->task_file_data & (AHCI_ATA_DEV_DRQ | AHCI_ATA_DEV_BUSY)) && spin--);

	if(spin == 0) {
		printf("AHCI: port is hung\n");
		return -1;
	}

	port->command_issue = 1 << slot;

	while(1) {
		if((port->command_issue & (1 << slot)) == 0) {
			break;
		}

		if(port->interrupt_state & AHCI_HBA_PxIS_TFES) {
			printf("AHCI: Fatal disk flush error\n");
			return -1;
		}
	}


	return 0;
}

int8_t ahci_identify(uint64_t disk_id) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	ahci_ata_identify_data_t identify_data;

	port->interrupt_state = (uint32_t)-1;

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

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)&identify_data;
	cmd_table->prdt_entry[0].data_byte_count = 511;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;
	fis->command = AHCI_ATA_CMD_IDENTIFY;

	uint32_t spin = 1000000;

	while((port->task_file_data & (AHCI_ATA_DEV_DRQ | AHCI_ATA_DEV_BUSY)) && spin--);

	if(spin == 0) {
		printf("AHCI: port is hung\n");
		return -1;
	}

	port->command_issue = 1 << slot;

	while(1) {
		if((port->command_issue & (1 << slot)) == 0) {
			break;
		}

		if(port->interrupt_state & AHCI_HBA_PxIS_TFES) {
			printf("AHCI: Fatal disk identify error\n");
			return -1;
		}
	}

	disk->cylinders = identify_data.cylinders;
	disk->heads = identify_data.heads;
	disk->sectors = identify_data.sectors_per_track;

	if(1 & (identify_data.command_set_supported >> 26)) {
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

	printf("AHCI: Info disk %li cyl %x head %x sec %x lba %lx serial %s model %s\n", disk->disk_id, disk->cylinders, disk->heads,
	       disk->sectors, disk->lba_count, disk->serial, disk->model);

	return 0;
}

int8_t ahci_read(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	printf("AHCI: Info read from port 0x%p at lba 0x%lx with size 0x%x to buffer 0x%p\n", port, lba, size, buffer);

	port->interrupt_state = (uint32_t)-1;

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

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)buffer;
	cmd_table->prdt_entry[0].data_byte_count = size - 1;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;
	fis->command = AHCI_ATA_CMD_READ_DMA_EX;
	fis->lba0 = lba & 0xFFF;
	fis->lba1 = (lba >> 24) & 0xFFF;
	fis->device = 1 << 6;
	fis->count = size / 512;

	uint32_t spin = 1000000;

	while((port->task_file_data & (AHCI_ATA_DEV_DRQ | AHCI_ATA_DEV_BUSY)) && spin--);

	if(spin == 0) {
		printf("AHCI: port is hung\n");
		return -1;
	}

	port->command_issue = 1 << slot;

	while(1) {
		if((port->command_issue & (1 << slot)) == 0) {
			break;
		}

		if(port->interrupt_state & AHCI_HBA_PxIS_TFES) {
			printf("AHCI: Fatal disk read error\n");
			return -1;
		}
	}

	return 0;
}

int8_t ahci_write(uint64_t disk_id, uint64_t lba, uint16_t size, uint8_t* buffer) {
	ahci_sata_disk_t* disk = (ahci_sata_disk_t*)linkedlist_get_data_at_position(sata_ports, disk_id);

	ahci_hba_port_t* port = (ahci_hba_port_t*)disk->port_address;

	printf("AHCI: Info write to port 0x%p at lba 0x%lx with size 0x%x from buffer 0x%p\n", port, lba, size, buffer);

	port->interrupt_state = (uint32_t)-1;

	int8_t slot = ahci_find_command_slot(port, 32);

	if(slot == -1) {
		printf("AHCI: Fatal cannot find empty command slot\n");
		return -1;
	}

	ahci_hba_cmd_header_t* cmd_hdr = (ahci_hba_cmd_header_t*)port->command_list_base_address;
	cmd_hdr += slot;

	cmd_hdr->command_fis_length = sizeof(ahci_fis_reg_h2d_t) / sizeof(uint32_t);
	cmd_hdr->write_direction = 1;
	cmd_hdr->prdt_length = 1;

	ahci_hba_prdt_t* cmd_table = (ahci_hba_prdt_t*)cmd_hdr->prdt_base_address;
	memory_memclean(cmd_table, sizeof(ahci_hba_prdt_t) + (sizeof(ahci_hba_prdt_entry_t) * (  cmd_hdr->prdt_length - 1)));

	cmd_table->prdt_entry[0].data_base_address = (uint64_t)buffer;
	cmd_table->prdt_entry[0].data_byte_count = size - 1;

	ahci_fis_reg_h2d_t* fis = (ahci_fis_reg_h2d_t*)&cmd_table->command_fis;
	memory_memclean(fis, sizeof(ahci_fis_reg_h2d_t));

	fis->fis_type = AHCI_FIS_TYPE_REG_H2D;
	fis->control_or_command = 1;
	fis->command = AHCI_ATA_CMD_WRITE_DMA_EX;
	fis->lba0 = lba & 0xFFF;
	fis->lba1 = (lba >> 24) & 0xFFF;
	fis->device = 1 << 6;
	fis->count = size / 512;

	uint32_t spin = 1000000;

	while((port->task_file_data & (AHCI_ATA_DEV_DRQ | AHCI_ATA_DEV_BUSY)) && spin--);

	if(spin == 0) {
		printf("AHCI: port is hung\n");
		return -1;
	}

	port->command_issue = 1 << slot;

	while(1) {
		if((port->command_issue & (1 << slot)) == 0) {
			break;
		}

		if(port->interrupt_state & AHCI_HBA_PxIS_TFES) {
			printf("AHCI: Fatal disk write error\n");
			return -1;
		}
	}

	return ahci_flush(disk->disk_id);
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
	printf("AHCI: Info avail slots 0x%x\n", slots);

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
	// Wait until CR (bit15) is cleared
	while (port->command_and_status & AHCI_HBA_PxCMD_CR); // check command running

	// Set FRE (bit4) and ST (bit0)
	port->command_and_status |= AHCI_HBA_PxCMD_FRE;  // set fis receive enable
	port->command_and_status |= AHCI_HBA_PxCMD_ST;   // set command start

	port->sata_control = 1;

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
