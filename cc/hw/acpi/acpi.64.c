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
#include <linkedlist.h>
#include <cpu.h>
#include <strings.h>

acpi_aml_object_t* ACPI_PM1A_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_PM1B_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_RESET_REGISTER = NULL;
acpi_contex_t* ACPI_CONTEXT = NULL;

int8_t acpi_build_register(acpi_aml_object_t** reg, uint64_t address, uint8_t address_space, uint8_t bit_width, uint8_t bit_offset);
#define acpi_build_register_with_gas(reg, gas) acpi_build_register(reg, gas.address, gas.address_space, gas.bit_width, gas.bit_offset)

int8_t acpi_page_map_table_addresses(acpi_xrsdp_descriptor_t* xrsdp_desc);

int8_t acpi_reset(){
	if(ACPI_CONTEXT == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "acpi context null", 0);
		return -1;
	}

	if(ACPI_CONTEXT->acpi_parser_context == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "acpi parser context null", 0);
		return -1;
	}

	return acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, ACPI_CONTEXT->fadt->reset_value, ACPI_RESET_REGISTER);
}

int8_t acpi_poweroff(){
	if(ACPI_CONTEXT == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "acpi context null", 0);
		return -1;
	}

	if(ACPI_CONTEXT->acpi_parser_context == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "acpi parser context null", 0);
		return -1;
	}

	if(ACPI_PM1A_CONTROL_REGISTER == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "pm1a control register is null", 0);
		return -1;
	}

	acpi_aml_object_t* s5 = acpi_aml_symbol_lookup(ACPI_CONTEXT->acpi_parser_context, "\\_S5_");

	if(s5 == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "No s5 state", 0);
	} else if(s5->type != ACPI_AML_OT_PACKAGE) {
		PRINTLOG(ACPI, LOG_ERROR, "s5 wrong object type: %i", s5->type);
	} else {
		acpi_aml_object_t* slp_type_a = linkedlist_get_data_at_position(s5->package.elements, 0);
		acpi_aml_object_t* slp_type_b = linkedlist_get_data_at_position(s5->package.elements, 1);

		if(slp_type_a == NULL || slp_type_b == NULL) {
			PRINTLOG(ACPI, LOG_ERROR, "s5 sleep type a 0x%lp or b 0x%lp is null", slp_type_a, slp_type_b);
		} else {
			int64_t slp_type_a_val = 0, slp_type_b_val = 0;

			if(acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_a, &slp_type_a_val) == 0 &&
			   acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_b, &slp_type_b_val) == 0) {


				PRINTLOG(ACPI, LOG_DEBUG, "acpi poweroff started", 0);

				acpi_pm1_control_register_t* reg = memory_malloc(sizeof(acpi_pm1_control_register_t));
				reg->sleep_enable = 1;
				reg->sleep_type = slp_type_a_val;

				uint16_t reg_val = *((uint16_t*)reg);

				PRINTLOG(ACPI, LOG_TRACE, "writing pm1a 0x%x", reg_val);

				if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val, ACPI_PM1A_CONTROL_REGISTER) != 0) {
					PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1a", 0);
					memory_free(reg);
				} else {
					if(ACPI_PM1B_CONTROL_REGISTER) {
						reg->sleep_type = slp_type_b_val;
						reg_val = *((uint16_t*)reg);

						PRINTLOG(ACPI, LOG_TRACE, "writing pm1b 0x%x", reg_val);

						if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val, ACPI_PM1B_CONTROL_REGISTER) != 0) {
							PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1b", 0);
							memory_free(reg);
						} else {
							cpu_hlt();
						}
					} else {
						cpu_hlt();
					}
				}

			} else {
				PRINTLOG(ACPI, LOG_ERROR, "Cannot obtain s5 sleep type", 0);
			}

		}
	}

	PRINTLOG(ACPI, LOG_FATAL, "acpi poweroff failed", 0);

	return -1;
}

int8_t acpi_build_register(acpi_aml_object_t** reg, uint64_t address, uint8_t address_space, uint8_t bit_width, uint8_t bit_offset){
	if(reg == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "register address null", 0);
		return -1;
	}

	if(bit_width == 0) {
		PRINTLOG(ACPI, LOG_ERROR, "bit width zero", 0);
		return -1;
	}

	if(address == 0) {
		PRINTLOG(ACPI, LOG_ERROR, "register address zero", 0);
		return -1;
	}

	*reg = memory_malloc(sizeof(acpi_aml_object_t));

	if((*reg) == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot allocate register memory", 0);
		return -1;
	}

	acpi_aml_object_t* opreg = memory_malloc(sizeof(acpi_aml_object_t));

	if(opreg == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot allocate opregion memory", 0);
		memory_free(*reg);
		return -1;
	}

	opreg->type = ACPI_AML_OT_OPREGION;
	opreg->opregion.region_space = address_space;
	opreg->opregion.region_offset = address;
	opreg->opregion.region_len = bit_width / 8;

	PRINTLOG(ACPI, LOG_TRACE, "register address 0x%lx type %i len 0x%lx", opreg->opregion.region_offset, opreg->opregion.region_space, opreg->opregion.region_len);

	acpi_aml_object_t* tmp = *reg;

	tmp->type = ACPI_AML_OT_FIELD;
	tmp->field.offset = bit_offset;
	tmp->field.sizeasbit = bit_width;
	tmp->field.access_type = bit_width / 8;

	if(tmp->field.access_type == 4) {
		tmp->field.access_type = 3;
	} else if(tmp->field.access_type == 8) {
		tmp->field.access_type = 4;
	}


	PRINTLOG(ACPI, LOG_TRACE, "field offset 0x%lx size %i at %i", tmp->field.offset, tmp->field.sizeasbit, tmp->field.access_type);

	tmp->field.related_object = opreg;

	return 0;
}

acpi_xrsdp_descriptor_t* acpi_find_xrsdp(){

	frame_t* acpi_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, SYSTEM_INFO->acpi_table);

	if(acpi_frames == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot find acpi frames of table 0x%016lx", SYSTEM_INFO->acpi_table);
		return NULL;
	}

	uint64_t acpi_frames_vas = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(acpi_frames->frame_address);

	PRINTLOG(ACPI, LOG_DEBUG, "acpi area frames 0x%016lx->0x%016lx 0x%08x", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);

	if(memory_paging_add_va_for_frame(acpi_frames_vas, acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot add page mapping for acpi area 0x%016lx->0x%016lx 0x%08x", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);
		return NULL;
	}

	acpi_xrsdp_descriptor_t* desc = (acpi_xrsdp_descriptor_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(SYSTEM_INFO->acpi_table);
	PRINTLOG(ACPI, LOG_DEBUG, "acpi descriptor address 0x%016lx", desc);


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

		acpi_page_map_table_addresses(desc);

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


int8_t acpi_page_map_table_addresses(acpi_xrsdp_descriptor_t* xrsdp_desc){
	if(xrsdp_desc->rsdp.revision == 0) {
		uint32_t addr = xrsdp_desc->rsdp.rsdt_address;
		acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(addr));
		uint8_t* table_addrs = (uint8_t*)(rsdt + 1);
		size_t table_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
		acpi_sdt_header_t* res;

		for(size_t i = 0; i < table_count; i++) {
			uint32_t table_addr = *((uint32_t*)(table_addrs + (i * sizeof(uint32_t))));
			res = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(table_addr));

			PRINTLOG(ACPI, LOG_TRACE, "table %i of %i at fa 0x%lp va 0x%lp", i, table_addr, res);

			frame_t* acpi_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)(uint64_t)table_addr);

			if(acpi_frames == NULL) {
				PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of table 0x%016lx", table_addr);
			} else {
				PRINTLOG(ACPI, LOG_TRACE, "frames of table 0x%016lx is 0x%lx 0x%lx", table_addr, acpi_frames->frame_address, acpi_frames->frame_count);
				memory_paging_add_va_for_frame((uint64_t)res, acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
				char_t* sign = strndup(res->signature, 4);
				PRINTLOG(ACPI, LOG_TRACE, "table name %s", sign);
				memory_free(sign);
			}

		}
	} else if (xrsdp_desc->rsdp.revision == 2) {
		acpi_xrsdt_t* xrsdt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(xrsdp_desc->xrsdt);

		size_t table_count = (xrsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(void*);
		acpi_sdt_header_t* res;

		for(size_t i = 0; i < table_count; i++) {
			res = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(xrsdt->acpi_sdt_header_ptrs[i]);

			PRINTLOG(ACPI, LOG_TRACE, "table %i of %i at fa 0x%lp va 0x%lp", i, table_count, xrsdt->acpi_sdt_header_ptrs[i], res);


			frame_t* acpi_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, xrsdt->acpi_sdt_header_ptrs[i]);

			if(acpi_frames == NULL) {
				PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of table 0x%016lx", xrsdt->acpi_sdt_header_ptrs[i]);
			} else {
				PRINTLOG(ACPI, LOG_TRACE, "frames of table 0x%016lx is 0x%lx 0x%lx", xrsdt->acpi_sdt_header_ptrs[i], acpi_frames->frame_address, acpi_frames->frame_count);
				memory_paging_add_va_for_frame((uint64_t)res, acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
				acpi_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
				char_t* sign = strndup(res->signature, 4);
				PRINTLOG(ACPI, LOG_TRACE, "table name %s", sign);
				memory_free(sign);
			}
		}
	}

	acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(xrsdp_desc, "FACP");

	if(fadt == NULL) {
		return -1;
	}

	uint64_t dsdt_fa = 0;

	if(SYSTEM_INFO->acpi_version == 2) {
		dsdt_fa = fadt->dsdt_address_64bit;
	} else {
		dsdt_fa = fadt->dsdt_address_32bit;
	}

	uint64_t dsdt_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dsdt_fa);

	frame_t* acpi_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)dsdt_fa);

	if(acpi_frames == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of  dsdt table", 0);
	} else {
		PRINTLOG(ACPI, LOG_TRACE, "frames of dsdt table is 0x%lx 0x%lx", acpi_frames->frame_address, acpi_frames->frame_count);
		memory_paging_add_va_for_frame(dsdt_va, acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
	}


	return 0;
}

acpi_sdt_header_t* acpi_get_next_table(acpi_xrsdp_descriptor_t* xrsdp_desc, char_t* signature, linkedlist_t old_tables) {
	if(xrsdp_desc->rsdp.revision == 0) {
		uint32_t addr = xrsdp_desc->rsdp.rsdt_address;
		acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(addr));
		uint8_t* table_addrs = (uint8_t*)(rsdt + 1);
		size_t table_count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
		acpi_sdt_header_t* res;

		for(size_t i = 0; i < table_count; i++) {
			uint32_t table_addr = *((uint32_t*)(table_addrs + (i * sizeof(uint32_t))));
			res = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA((uint64_t)(table_addr));

			PRINTLOG(ACPI, LOG_TRACE, "looking for table %i of %i at fa 0x%lp va 0x%lp", i, table_addr, res);

			if(memory_memcompare(res->signature, signature, 4) == 0) {

				if(old_tables && linkedlist_contains(old_tables, res) == 0) {
					continue;
				}

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

			PRINTLOG(ACPI, LOG_TRACE, "looking for table %i of %i at fa 0x%lp va 0x%lp", i, table_count, xrsdt->acpi_sdt_header_ptrs[i], res);

			if(memory_memcompare(res->signature, signature, 4) == 0) {

				if(old_tables && linkedlist_contains(old_tables, res) == 0) {
					continue;
				}

				if(acpi_validate_checksum(res) == 0) {
					return res;
				}

				return NULL;
			}
		}
	}

	return NULL;
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
	ACPI_CONTEXT = memory_malloc(sizeof(acpi_contex_t));

	if(ACPI_CONTEXT == NULL) {
		return -1;
	}

	ACPI_CONTEXT->xrsdp_desc = desc;

	acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(desc, "FACP");

	if(fadt == NULL) {
		PRINTLOG(ACPI, LOG_DEBUG, "fadt not found", 0);

		return -1;
	}

	ACPI_CONTEXT->fadt = fadt;

	PRINTLOG(ACPI, LOG_DEBUG, "fadt found", 0);

	int8_t acpi_enabled = -1;

	if(fadt->smi_command_port == 0) {
		PRINTLOG(ACPI, LOG_DEBUG, "smi command port is 0.", 0);
		acpi_enabled = 0;
	}

	if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi enable/disable is 0.", 0);
		acpi_enabled = 0;
	}

	acpi_build_register_with_gas(&ACPI_RESET_REGISTER, fadt->reset_reg);

	if(SYSTEM_INFO->acpi_version == 2) {
		acpi_build_register_with_gas(&ACPI_PM1A_CONTROL_REGISTER, fadt->pm_1a_control_block_address_64bit);

		if(fadt->pm_1b_control_block_address_64bit.address) {
			acpi_build_register_with_gas(&ACPI_PM1B_CONTROL_REGISTER, fadt->pm_1b_control_block_address_64bit);
		}
	} else {
		acpi_build_register(&ACPI_PM1A_CONTROL_REGISTER, fadt->pm_1a_control_block_address_32bit, 1, 16, 0);
		acpi_build_register(&ACPI_PM1B_CONTROL_REGISTER, fadt->pm_1b_control_block_address_32bit, 1, 16, 0);
	}

	uint16_t pm_1a_port = fadt->pm_1a_control_block_address_64bit.address;
	uint32_t pm_1a_value = 0;

	if((pm_1a_value & 0x1) == 0x1) {
		PRINTLOG(ACPI, LOG_DEBUG, "pm 1a control block acpi en is setted", 0);
		acpi_enabled = 0;
	}

	if(acpi_enabled != 0) {
		outb(fadt->smi_command_port, fadt->acpi_enable);

		while((inw(pm_1a_port) & 0x1) != 0x1);
	}

	uint16_t pm_1b_port = fadt->pm_1b_control_block_address_64bit.address;

	if(pm_1b_port) {
		uint32_t pm_1b_value = inw(pm_1b_port);

		if((pm_1b_value & 0x1) == 0x1) {
			PRINTLOG(ACPI, LOG_DEBUG, "pm 1b control block acpi en is setted", 0);
			acpi_enabled = 0;
		}
	}

	PRINTLOG(ACPI, LOG_INFO, "ACPI Enabled", 0);

	uint64_t dsdt_fa = 0;

	if(SYSTEM_INFO->acpi_version == 2) {
		dsdt_fa = fadt->dsdt_address_64bit;
	} else {
		dsdt_fa = fadt->dsdt_address_32bit;
	}

	acpi_sdt_header_t* dsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dsdt_fa);
	PRINTLOG(ACPI, LOG_DEBUG, "DSDT address 0x%016lx", dsdt);

	acpi_aml_parser_context_t* pctx = acpi_aml_parser_context_create_with_heap(NULL, dsdt->revision);

	if(pctx == NULL) {
		PRINTLOG(ACPI, LOG_ERROR, "aml parser creation failed", 0);

		return -1;
	}

	ACPI_CONTEXT->acpi_parser_context = pctx;

	if(acpi_aml_parser_parse_table(pctx, dsdt) == 0) {
		PRINTLOG(ACPI, LOG_INFO, "dsdt table parsed as aml", 0);
	} else {
		PRINTLOG(ACPI, LOG_ERROR, "dsdt table cannot parsed as aml", 0);
		return -1;
	}

	acpi_sdt_header_t* ssdt = acpi_get_table(desc, "SSDT");

	uint32_t ssdt_cnt = 0;

	linkedlist_t old_ssdts = linkedlist_create_list();

	while(ssdt) {
		if(acpi_aml_parser_parse_table(pctx, ssdt) == 0) {
			PRINTLOG(ACPI, LOG_INFO, "ssdt%i table parsed as aml", ssdt_cnt);
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "ssdt%i table cannot parsed as aml", ssdt_cnt);
			return -1;
		}

		ssdt_cnt++;

		linkedlist_list_insert(old_ssdts, ssdt);

		ssdt = acpi_get_next_table(desc, "SSDT", old_ssdts);
	}

	linkedlist_destroy(old_ssdts);

	if(acpi_device_build(pctx) != 0) {
		PRINTLOG(ACPI, LOG_ERROR, "devices cannot be builded", 0);
		return -1;
	}

	LOGBLOCK(ACPI, LOG_TRACE){
		acpi_device_print_all(pctx);
		acpi_aml_print_symbol_table(pctx);
	}

	if(acpi_device_init(pctx) != 0) {
		PRINTLOG(ACPI, LOG_ERROR, "devices cannot be initialized", 0);
		return -1;
	}

	PRINTLOG(ACPI, LOG_INFO, "Devices initialized", 0);

	if(acpi_device_reserve_memory_ranges(pctx) != 0) {
		PRINTLOG(ACPI, LOG_ERROR, "devices memory reservation failed", 0);
		return -1;
	}

	if(acpi_build_interrupt_map(pctx) != 0) {
		PRINTLOG(ACPI, LOG_ERROR, "cannot build interrupt map", 0);
		return -1;
	}

	PRINTLOG(ACPI, LOG_INFO, "Interrupt map builded", 0);

	ACPI_CONTEXT->mcfg = (acpi_table_mcfg_t*)acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "MCFG");

	PRINTLOG(ACPI, LOG_INFO, "acpi init completed successfully", 0);

	return 0;
}
