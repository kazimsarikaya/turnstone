/**
 * @file acpi.h
 * @brief acpi interface
 */
#ifndef ___ACPI_H
/*! prevent duplicate header error macro */
#define ___ACPI_H 0

#include <types.h>

/*! acpi rsdp signature at memory. spaces are important */
#define ACPI_RSDP_SIGNATURE "RSD PTR "


typedef struct {
	char_t signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char_t oem_id[6];
	char_t oem_table_id[8];
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
}__attribute__((packed)) acpi_sdt_header_t;

typedef struct {
	acpi_sdt_header_t header;
	acpi_sdt_header_t* acpi_sdt_header_ptrs[];
}__attribute__((packed)) acpi_xrsdt_t;

typedef struct {
	acpi_sdt_header_t header;
	uint64_t reserved0;
	struct {
		uint64_t base_address;
		uint16_t group_number;
		uint8_t bus_start;
		uint8_t bus_end;
		uint32_t reserved0;
	}pci_segment_group_config[];
}__attribute__((packed)) acpi_table_mcfg_t;

typedef struct {
	char_t signature[8];
	uint8_t checksum;
	char_t oem_id[6];
	uint8_t revision;
	uint32_t rsdt_address;
	uint32_t length;
	acpi_xrsdt_t* xrsdt;
	uint8_t extended_checksum;
	uint8_t reserved[3];
}__attribute__((packed)) acpi_xrsdp_descriptor_t;

acpi_xrsdp_descriptor_t* acpi_find_xrsdp();
uint8_t acpi_validate_checksum(acpi_sdt_header_t* sdt_header);

acpi_sdt_header_t* acpi_get_table(acpi_xrsdp_descriptor_t* xrsdp_desc, char_t* signature);

acpi_table_mcfg_t* acpi_get_mcfg_table(acpi_xrsdp_descriptor_t* xrsdp_desc);

#endif
