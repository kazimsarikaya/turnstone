/**
 * @file pci.h
 * @brief pci interface
 */
#ifndef ___PCI_H
/*! prevent duplicate header error macro */
#define ___PCI_H 0

#include <types.h>
#include <iterator.h>
#include <acpi.h>
#include <memory.h>

#define PCI_HEADER_TYPE_GENERIC_DEVICE 0x0
#define PCI_HEADER_TYPE_PCI2PCI_BRIDGE 0x1
#define PCI_HEADER_TYPE_CARDBUS_BRIDGE 0x2

#define PCI_DEVICE_MAX_COUNT 32 ///< max device count per bus
#define PCI_FUNCTION_MAX_COUNT 8 ///< max function count per device

#define PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER  0x01

#define PCI_DEVICE_SUBCLASS_SATA_CONTROLLER  0x06


typedef struct {
	uint8_t io_space : 1;
	uint8_t memory_space : 1;
	uint8_t bus_master : 1;
	uint8_t special_cycles : 1;
	uint8_t memory_write_and_invalidate_enable : 1;
	uint8_t vga_palette_snoop : 1;
	uint8_t parity_error_response : 1;
	uint8_t reserved0 : 1;
	uint8_t serr_enable : 1;
	uint8_t fast_back2back_enable : 1;
	uint8_t interrupt_disable : 1;
	uint8_t reserved1 : 5;
} __attribute__((packed)) pci_command_register_t;

typedef struct {
	uint8_t reserved0 : 3;
	uint8_t interrupt_status : 1;
	uint8_t capabilities_list : 1;
	uint8_t mhz66_capable : 1;
	uint8_t reserved1 : 1;
	uint8_t fast_back2back_capable : 1;
	uint8_t master_data_parity_error : 1;
	uint8_t devsel_timing : 2;
	uint8_t signaled_target_abort : 1;
	uint8_t received_target_abort : 1;
	uint8_t received_master_abort : 1;
	uint8_t signaled_system_error : 1;
	uint8_t detected_parity_error : 1;
} __attribute__((packed)) pci_status_register_t;

/**
 * @struct pci_header_type_register_t
 * @brief pci device type data inside pci_common_header_t
 */
typedef struct {
	uint8_t header_type : 7; ///< device types: 0x00 generic, 0x01 bridge, 0x02 cardbus
	uint8_t multifunction : 1; ///< if 1 then device is multi-function device and has 8 functions, else only function 0.
} __attribute__((packed)) pci_header_type_register_t; ///< short hand for struct

typedef struct {
	uint8_t completion_code : 4;
	uint8_t reserved0 : 2;
	uint8_t start_bist : 1;
	uint8_t bist_capable : 1;
} __attribute__((packed)) pci_bist_register_t;


typedef union {
	struct {
		uint8_t type : 1;
		uint32_t reserved0 : 31;
	} bar_type;
	struct {
		uint8_t bar_type : 1;
		uint8_t type : 2;
		uint8_t prefetchable : 1;
		uint32_t base_address : 28;
	} memory_space_bar;
	struct {
		uint8_t bar_type : 1;
		uint8_t reserved0 : 1;
		uint32_t base_address : 30;
	} io_space_bar;
} __attribute__((packed)) pci_bar_register_t;

typedef struct {
	uint16_t vendor_id : 16;
	uint16_t device_id : 16;
	pci_command_register_t command;
	pci_status_register_t status;
	uint8_t revsionid : 8;
	uint8_t prog_if : 8;
	uint8_t subclass_code : 8;
	uint8_t class_code : 8;
	uint8_t cache_line_size : 8;
	uint8_t latency_timer : 8;
	pci_header_type_register_t header_type;
	pci_bist_register_t bist;
} __attribute__((packed)) pci_common_header_t;

typedef struct {
	pci_common_header_t common_header;
	pci_bar_register_t bar0;
	pci_bar_register_t bar1;
	pci_bar_register_t bar2;
	pci_bar_register_t bar3;
	pci_bar_register_t bar4;
	pci_bar_register_t bar5;
	uint32_t cardbus_cis_pointer : 32;
	uint16_t subsystem_vendor_id : 16;
	uint16_t subsystem_id : 16;
	uint32_t expension_rom_base_address : 32;
	uint8_t capabilities_pointer : 8;
	uint32_t reserved0 : 24;
	uint32_t reserved1 : 32;
	uint8_t interrupt_line : 8;
	uint8_t interrupt_pin : 8;
	uint8_t min_grant : 8;
	uint8_t max_latency : 8;
} __attribute__((packed)) pci_generic_device_t;


typedef struct {
	pci_common_header_t common_header;
	pci_bar_register_t bar0;
	pci_bar_register_t bar1;
	uint8_t primary_bus_number : 8;
	uint8_t secondary_bus_number : 8;
	uint8_t subordinate_bus_number : 8;
	uint8_t secondary_latency_timer : 8;
	uint8_t io_base : 8;
	uint8_t io_limit : 8;
	pci_status_register_t secondary_status;
	uint16_t memory_base : 16;
	uint16_t memory_limit : 16;
	uint16_t prefetchable_memory_base : 16;
	uint16_t prefetchable_memory_limit : 16;
	uint32_t prefetchable_base_upper_32bits : 32;
	uint32_t prefetchable_limit_upper_32bits : 32;
	uint16_t io_base_upper_16bits : 16;
	uint16_t io_limit_upper_16bits : 16;
	uint8_t capabilities_pointer : 8;
	uint32_t reserved0 : 24;
	uint32_t expension_rom_base_address : 32;
	uint8_t interrupt_line : 8;
	uint8_t interrupt_pin : 8;
	uint16_t bridge_control : 16;
} __attribute__((packed)) pci_pci2pci_bridge_t;


typedef struct {
	pci_common_header_t common_header;
	uint32_t cardbus_socket_or_exca_base_address : 32;
	uint8_t capabilities_pointer : 8;
	uint8_t reserved0 : 8;
	pci_status_register_t secondary_status;
	uint8_t pci_bus_number : 8;
	uint8_t cardbus_bus_number : 8;
	uint8_t subordinate_bus_number : 8;
	uint8_t cardbus_latency_timer : 8;
	uint32_t memory_base_address0 : 32;
	uint32_t memory_limit0 : 32;
	uint32_t memory_base_address1 : 32;
	uint32_t memory_limit1 : 32;
	uint32_t io_base_address0 : 32;
	uint32_t io_limit0 : 32;
	uint32_t io_base_address1 : 32;
	uint32_t io_limit1 : 32;
	uint8_t interrupt_line : 8;
	uint8_t interrupt_pin : 8;
	uint16_t bridge_control : 16;
	uint16_t subsystem_vendor_id : 16;
	uint16_t subsystem_device_id : 16;
	uint32_t pccard_16bit_legacy_mode_base_address : 32;
} __attribute__((packed)) pci_cardbus_bridge_t;

/**
 * @struct pci_dev_t
 * @brief the pci device info returned by the iterator
 */
typedef struct {
	uint16_t group_number; ///< device's bus group number.
	uint8_t bus_number; ///< bus number of the device
	uint8_t device_number; ///< device number
	uint8_t function_number; ///< device function number
	pci_common_header_t* pci_header; ///< pci generic memory area
} pci_dev_t; ///< short hand for struct

/**
 * @brief creates an iterator over pci device devices at mcfg memory area
 * @param  heap iterator of heap
 * @param  mcfg mcfg memory area which indentified by acpi table
 * @return      iterator
 */
iterator_t* pci_iterator_create_with_heap(memory_heap_t* heap, acpi_table_mcfg_t* mcfg);
/*! creates pci iterator at default heap */
#define pci_iterator_create(mcfg) pci_iterator_create_with_heap(NULL, mcfg)

typedef struct {
	uint8_t capability_id : 8;
	uint8_t next_pointer : 8;
} __attribute__((packed)) pci_capability_t;

#endif
