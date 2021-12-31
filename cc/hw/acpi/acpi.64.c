/**
 * @file acpi.64.c
 * @brief acpi table parsers
 */
#include <acpi.h>
#include <memory.h>
#include <video.h>
#include <ports.h>
#include <acpi/aml.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <systeminfo.h>

acpi_xrsdp_descriptor_t* acpi_find_xrsdp(){

	frame_t* acpi_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, SYSTEM_INFO->acpi_table);

	if(acpi_frames == NULL) {
		PRINTLOG("ACPI", "ERROR", "cannot find acpi frames of table 0x%016lx", SYSTEM_INFO->acpi_table);
		return NULL;
	}

	uint64_t acpi_frames_vas = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(acpi_frames->frame_address);

	PRINTLOG("ACPI", "DEBUG", "acpi area frames 0x%016lx->0x%016lx 0x%08x", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);

	if(memory_paging_add_va_for_frame(acpi_frames_vas, acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		PRINTLOG("ACPI", "ERROR", "cannot add page mapping for acpi area 0x%016lx->0x%016lx 0x%08x", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);
		return NULL;
	}

	acpi_xrsdp_descriptor_t* desc = (acpi_xrsdp_descriptor_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(SYSTEM_INFO->acpi_table);
	PRINTLOG("ACPI", "DEBUG", "acpi descriptor address 0x%016lx", desc);


	if(desc != NULL) {
		size_t len = 0;

		if(desc->rsdp.revision == 0) {
			len = sizeof(acpi_rsdp_descriptor_t);
		} else if(desc->rsdp.revision == 2) {
			len = desc->length;
		} else {
			return NULL;
		}

		uint8_t* data2csum = (uint8_t*)desc;
		uint8_t checksum = 0;

		for(size_t i = 0; i < len; i++) {
			checksum += data2csum[i];
		}

		if(checksum != 0x00) {
			return NULL;
		}

		return desc;
	}

	return NULL;
}


uint8_t acpi_validate_checksum(acpi_sdt_header_t* sdt_header){
	uint8_t* data = (uint8_t*)sdt_header;
	uint8_t checksum = 0;

	for(size_t i = 0; i < sdt_header->length; i++) {
		checksum += data[i];
	}

	return checksum;
}

acpi_sdt_header_t* acpi_get_table(acpi_xrsdp_descriptor_t* xrsdp_desc, char_t* signature){
	if(xrsdp_desc->rsdp.revision == 0) {
		uint32_t addr = xrsdp_desc->rsdp.rsdt_address;
		acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(addr));
		uint8_t* table_addrs = (uint8_t*)(rsdt + 1);
		size_t table_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
		acpi_sdt_header_t* res;

		for(size_t i = 0; i < table_count; i++) {
			uint32_t table_addr = *((uint32_t*)(table_addrs + (i * sizeof(uint32_t))));
			res = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(table_addr));

			if(memory_memcompare(res->signature, signature, 4) == 0) {
				if(acpi_validate_checksum(res) == 0) {
					return res;
				}

				return NULL;
			}

		}
	} else if (xrsdp_desc->rsdp.revision == 2) {
		acpi_xrsdt_t* xrsdt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(xrsdp_desc->xrsdt);

		size_t table_count = (xrsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(void*);
		acpi_sdt_header_t* res;

		for(size_t i = 0; i < table_count; i++) {
			res = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(xrsdt->acpi_sdt_header_ptrs[i]);
			if(memory_memcompare(res->signature, signature, 4) == 0) {
				if(acpi_validate_checksum(res) == 0) {
					return res;
				}

				return NULL;
			}
		}
	}

	return NULL;
}


acpi_table_mcfg_t* acpi_get_mcfg_table(acpi_xrsdp_descriptor_t* xrsdp_desc){
	return (acpi_table_mcfg_t*)acpi_get_table(xrsdp_desc, "MCFG");
}

linkedlist_t acpi_get_apic_table_entries_with_heap(memory_heap_t* heap, acpi_sdt_header_t* sdt_header){
	if(memory_memcompare(sdt_header->signature, "APIC", 4) != 0) {
		return NULL;
	}

	linkedlist_t entries = linkedlist_create_list_with_heap(heap);
	acpi_table_madt_entry_t* e;
	uint8_t* data = (uint8_t*)sdt_header;
	uint8_t* data_end = data + sdt_header->length;

	data += sizeof(acpi_sdt_header_t);
	e = memory_malloc_ext(heap, sizeof(acpi_table_madt_entry_t), 0x0);
	e->local_apic_address.type = ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS;
	e->local_apic_address.length = 10;
	e->local_apic_address.address = (uint32_t)(*((uint32_t*)data));
	data += sizeof(uint32_t);
	e->local_apic_address.flags = (uint32_t)(*((uint32_t*)data));
	data += sizeof(uint32_t);
	linkedlist_list_insert(entries, e);

	while(data < data_end) {
		e = (acpi_table_madt_entry_t*)data;
		linkedlist_list_insert(entries, e);
		data += e->info.length;

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE) {
			acpi_table_madt_entry_t* t_e = linkedlist_delete_at_position(entries, 0);
			memory_free_ext(heap, t_e);
		}

	}

	return entries;
}

int8_t acpi_setup(acpi_xrsdp_descriptor_t* desc) {
	acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(desc, "FACP");

	if(fadt == NULL) {
		PRINTLOG("ACPI", "DEBUG", "fadt not found", 0);

		return -1;
	}

	PRINTLOG("ACPI", "DEBUG", "fadt found", 0);

	int8_t acpi_enabled = -1;

	if(fadt->smi_command_port == 0) {
		PRINTLOG("ACPI", "DEBUG", "cpi command port is 0.", 0);
		acpi_enabled = 0;
	}

	if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
		PRINTLOG("ACPI", "DEBUG", "acpi enable/disable is 0.", 0);
		acpi_enabled = 0;
	}

	uint16_t pm_1a_port = fadt->pm_1a_control_block_address_64bit.address;
	uint32_t pm_1a_value = inw(pm_1a_port);

	if((pm_1a_value & 0x1) == 0x1) {
		PRINTLOG("ACPI", "DEBUG", "pm 1a control block acpi en is setted", 0);
		acpi_enabled = 0;
	}

	if(acpi_enabled != 0) {
		outb(fadt->smi_command_port, fadt->acpi_enable);

		while((inw(pm_1a_port) & 0x1) != 0x1);
	}

	PRINTLOG("ACPI", "INFO", "ACPI Enabled", 0);

	uint64_t dsdt_fa = 0;

	if(SYSTEM_INFO->acpi_version == 2) {
		dsdt_fa = fadt->dsdt_address_64bit;
	} else {
		dsdt_fa = fadt->dsdt_address_32bit;
	}

	acpi_sdt_header_t* dsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dsdt_fa);
	PRINTLOG("ACPI", "DEBUG", "DSDT address 0x%016lx", dsdt);


	if(acpi_validate_checksum(dsdt) != 0) {
		printf("dsdt not ok\n");

		return -1;
	}

	int64_t aml_size = dsdt->length - sizeof(acpi_sdt_header_t);
	uint8_t* aml = (uint8_t*)(dsdt + 1);

	acpi_aml_parser_context_t* pctx = acpi_aml_parser_context_create_with_heap(NULL, dsdt->revision, aml, aml_size);

	if(pctx == NULL) {
		printf("aml parser creation failed\n");

		return -1;
	}

	int res = -1;

	if(acpi_aml_parse(pctx) == 0) {
		printf("aml parsed\n");
		res = 0;
	} else {
		printf("aml not parsed\n");
		return -1;
	}

	if(acpi_aml_build_devices(pctx) != 0) {
		PRINTLOG("ACPI", "ERROR", "devices cannot be builded", 0);
	}

	acpi_aml_print_devices(pctx);

	outl(0x0CD8, 0);   // qemu acpi init emulation

	return res;
}
