/**
 * @file acpi.h
 * @brief acpi interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___ACPI_H
/*! prevent duplicate header error macro */
#define ___ACPI_H 0

#include <types.h>
#include <list.h>

/*! acpi rsdp signature at memory. spaces are important */
#define ACPI_RSDP_SIGNATURE "RSD PTR "


typedef struct acpi_sdt_header_t {
    char_t   signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char_t   oem_id[6];
    char_t   oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
}__attribute__((packed)) acpi_sdt_header_t;

typedef struct acpi_xrsdt_t {
    acpi_sdt_header_t  header;
    acpi_sdt_header_t* acpi_sdt_header_ptrs[];
}__attribute__((packed)) acpi_xrsdt_t;

typedef struct acpi_table_mcfg_t {
    acpi_sdt_header_t header;
    uint64_t          reserved0;
    struct pci_segment_group_config_t {
        uint64_t base_address;
        uint16_t group_number;
        uint8_t  bus_start;
        uint8_t  bus_end;
        uint32_t reserved0;
    }pci_segment_group_config[];
}__attribute__((packed)) acpi_table_mcfg_t;

typedef struct acpi_rsdp_descriptor_t {
    char_t   signature[8];
    uint8_t  checksum;
    char_t   oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
}__attribute__((packed)) acpi_rsdp_descriptor_t;

typedef struct acpi_xrsdp_descriptor_t {
    acpi_rsdp_descriptor_t rsdp;
    uint32_t               length;
    acpi_xrsdt_t*          xrsdt;
    uint8_t                extended_checksum;
    uint8_t                reserved[3];
}__attribute__((packed)) acpi_xrsdp_descriptor_t;

typedef struct acpi_generic_address_structure_t {
    uint8_t  address_space;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
}__attribute__((packed)) acpi_generic_address_structure_t;

typedef struct acpi_table_fadt_t {
    acpi_sdt_header_t                header;
    uint32_t                         firmare_control_address_32bit;
    uint32_t                         dsdt_address_32bit;
    uint8_t                          reserved0;
    uint8_t                          preferred_power_management_profile;
    uint16_t                         sci_interrupt;
    uint32_t                         smi_command_port;
    uint8_t                          acpi_enable;
    uint8_t                          acpi_disable;
    uint8_t                          s4_bios_req;
    uint8_t                          pstate_control;
    uint32_t                         pm_1a_event_block_address_32bit;
    uint32_t                         pm_1b_event_block_address_32bit;
    uint32_t                         pm_1a_control_block_address_32bit;
    uint32_t                         pm_1b_control_block_address_32bit;
    uint32_t                         pm_2_control_block_address_32bit;
    uint32_t                         pm_timer_block_address_32bit;
    uint32_t                         gpe0_block_address_32bit;
    uint32_t                         gpe1_block_address_32bit;
    uint8_t                          pm_1_event_length;
    uint8_t                          pm_1_control_length;
    uint8_t                          pm_2_control_length;
    uint8_t                          pm_timer_length;
    uint8_t                          gpe0_length;
    uint8_t                          gpe1_length;
    uint8_t                          gpe1_base;
    uint8_t                          c_state_control;
    uint16_t                         worst_c2_latency;
    uint16_t                         worst_c3_latency;
    uint16_t                         flush_size;
    uint16_t                         flush_stride;
    uint8_t                          duty_offset;
    uint8_t                          duty_width;
    uint8_t                          day_alarm;
    uint8_t                          month_alarm;
    uint8_t                          century;
    uint16_t                         boot_architecture_flags;
    uint8_t                          reserved1;
    uint32_t                         flags;
    acpi_generic_address_structure_t reset_reg;
    uint8_t                          reset_value;
    uint8_t                          reserved2[3];
    uint64_t                         firmare_control_address_64bit;
    uint64_t                         dsdt_address_64bit;
    acpi_generic_address_structure_t pm_1a_event_block_address_64bit;
    acpi_generic_address_structure_t pm_1b_event_block_address_64bit;
    acpi_generic_address_structure_t pm_1a_control_block_address_64bit;
    acpi_generic_address_structure_t pm_1b_control_block_address_64bit;
    acpi_generic_address_structure_t pm_2_control_block_address_64bit;
    acpi_generic_address_structure_t pm_timer_block_address_64bit;
    acpi_generic_address_structure_t gpe0_block_address_64bit;
    acpi_generic_address_structure_t gpe1_block_address_64bit;
}__attribute__((packed)) acpi_table_fadt_t;

typedef struct acpi_pm1_control_register_t {
    uint16_t sci_enable   : 1;
    uint16_t bm_rld       : 1;
    uint16_t gbl_rls      : 1;
    uint16_t reserved0    : 6;
    uint16_t ignored      : 1;
    uint16_t sleep_type   : 3;
    uint16_t sleep_enable : 1;
    uint16_t reserved1    : 2;
}__attribute__((packed)) acpi_pm1_control_register_t;

typedef enum acpi_madt_entry_type_t {
    ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS=0xFF,
    ACPI_MADT_ENTRY_TYPE_PROCESSOR_LOCAL_APIC=0,
    ACPI_MADT_ENTRY_TYPE_IOAPIC=1,
    ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE=2,
    ACPI_MADT_ENTRY_TYPE_NMI=4,
    ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE=5
} acpi_madt_entry_type_t;

typedef union acpi_table_madt_entry_t {
    struct info_t {
        uint8_t type; ///< for casting used as 1 byte data
        uint8_t length;
    } info;
    struct local_apic_address_t {
        uint8_t  type; ///< for casting used as 1 byte data
        uint8_t  length;
        uint32_t address;
        uint32_t flags;
    } local_apic_address;
    struct processor_local_apic_t {
        uint8_t  type; ///< for casting used as 1 byte data
        uint8_t  length;
        uint8_t  processor_id;
        uint8_t  apic_id;
        uint32_t flags;
    } processor_local_apic;
    struct ioapic_t {
        uint8_t  type; ///< for casting used as 1 byte data
        uint8_t  length;
        uint8_t  ioapic_id;
        uint8_t  reserved0;
        uint32_t address;
        uint32_t global_system_interrupt_base;
    } ioapic;
    struct interrupt_source_override_t {
        uint8_t  type; ///< for casting used as 1 byte data
        uint8_t  length;
        uint8_t  bus_source;
        uint8_t  irq_source;
        uint32_t global_system_interrupt;
        uint16_t flags;
    } interrupt_source_override;
    struct nmi_t {
        uint8_t  type; ///< for casting used as 1 byte data
        uint8_t  length;
        uint8_t  processor_id;
        uint16_t flags;
        uint8_t  lint;
    } nmi;
    struct local_apic_address_override_t {
        uint16_t reserved0;
        uint64_t address;
    } local_apic_address_override;
}__attribute__((packed)) acpi_table_madt_entry_t;

typedef struct acpi_contex_t {
    acpi_xrsdp_descriptor_t* xrsdp_desc;
    acpi_table_fadt_t*       fadt;
    acpi_table_mcfg_t*       mcfg;
    void*                    acpi_parser_context;
} acpi_contex_t;

extern acpi_contex_t* ACPI_CONTEXT;

acpi_xrsdp_descriptor_t* acpi_find_xrsdp(void);
uint8_t                  acpi_validate_checksum(acpi_sdt_header_t* sdt_header);

acpi_sdt_header_t* acpi_get_next_table(acpi_xrsdp_descriptor_t* xrsdp_desc, const char_t* signature, list_t* old_tables);
#define acpi_get_table(d, s) acpi_get_next_table(d, s, NULL)

list_t* acpi_get_apic_table_entries_with_heap(memory_heap_t* heap, acpi_sdt_header_t* sdt_header);
#define acpi_get_apic_table_entries(sdt_hdr) acpi_get_apic_table_entries_with_heap(NULL, sdt_hdr)

int8_t acpi_setup(acpi_xrsdp_descriptor_t* desc);

int8_t acpi_reset(void);
int8_t acpi_poweroff(void);
int8_t acpi_setup_events(void);

#endif
