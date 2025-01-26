/**
 * @file pci.h
 * @brief pci interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___PCI_H
/*! prevent duplicate header error macro */
#define ___PCI_H 0

#include <types.h>
#include <iterator.h>
#include <acpi.h>
#include <memory.h>
#include <list.h>
#include <cpu/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCI_HEADER_TYPE_GENERIC_DEVICE 0x0
#define PCI_HEADER_TYPE_PCI2PCI_BRIDGE 0x1
#define PCI_HEADER_TYPE_CARDBUS_BRIDGE 0x2

#define PCI_DEVICE_MAX_COUNT 32 ///< max device count per bus
#define PCI_FUNCTION_MAX_COUNT 8 ///< max function count per device

#define PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER  0x01
#define PCI_DEVICE_CLASS_NETWORK_CONTROLLER       0x02
#define PCI_DEVICE_CLASS_DISPLAY_CONTROLLER       0x03
#define PCI_DEVICE_CLASS_BRIDGE_CONTROLLER        0x06
#define PCI_DEVICE_CLASS_SYSTEM_PERIPHERAL        0x08
#define PCI_DEVICE_CLASS_INPUT_DEVICE             0x09
#define PCI_DEVICE_CLASS_SERIAL_BUS               0x0C

#define PCI_DEVICE_SUBCLASS_USB_CONTROLLER   0x03
#define PCI_DEVICE_SUBCLASS_SATA_CONTROLLER  0x06
#define PCI_DEVICE_SUBCLASS_NVME_CONTROLLER  0x08
#define PCI_DEVICE_SUBCLASS_ETHERNET         0x00
#define PCI_DEVICE_SUBCLASS_VGA              0x00
#define PCI_DEVICE_SUBCLASS_BRIDGE_HOST      0x00
#define PCI_DEVICE_SUBCLASS_BRIDGE_ISA       0x01
#define PCI_DEVICE_SUBCLASS_BRIDGE_OTHER     0x80
#define PCI_DEVICE_SUBCLASS_SP_OTHER         0x80
#define PCI_DEVICE_SUBCLASS_USB              0x80

#define PCI_DEVICE_PROGIF_OHCI               0x10
#define PCI_DEVICE_PROGIF_EHCI               0x20
#define PCI_DEVICE_PROGIF_XHCI               0x30

#define PCI_DEVICE_CAPABILITY_AER     0x01
#define PCI_DEVICE_CAPABILITY_MSI     0x05
#define PCI_DEVICE_CAPABILITY_VENDOR  0x09
#define PCI_DEVICE_CAPABILITY_PCIE    0x10
#define PCI_DEVICE_CAPABILITY_MSIX    0x11

#define PCI_IO_PORT_CONFIG 0x0CF8
#define PCI_IO_PORT_DATA   0x0CFC

#define PCI_IO_PORT_CREATE_ADDRESS(bus, dev, func, offset) ((bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC) | 0x80000000UL)

int8_t   pci_io_port_write_data(uint32_t address, uint32_t data, uint8_t bc);
uint32_t pci_io_port_read_data(uint32_t address, uint8_t bc);

typedef struct pci_command_register_t {
    uint8_t io_space                           : 1;
    uint8_t memory_space                       : 1;
    uint8_t bus_master                         : 1;
    uint8_t special_cycles                     : 1;
    uint8_t memory_write_and_invalidate_enable : 1;
    uint8_t vga_palette_snoop                  : 1;
    uint8_t parity_error_response              : 1;
    uint8_t reserved0                          : 1;
    uint8_t serr_enable                        : 1;
    uint8_t fast_back2back_enable              : 1;
    uint8_t interrupt_disable                  : 1;
    uint8_t reserved1                          : 5;
} __attribute__((packed)) pci_command_register_t;

typedef struct pci_status_register_t {
    uint8_t reserved0                : 3;
    uint8_t interrupt_status         : 1;
    uint8_t capabilities_list        : 1;
    uint8_t mhz66_capable            : 1;
    uint8_t reserved1                : 1;
    uint8_t fast_back2back_capable   : 1;
    uint8_t master_data_parity_error : 1;
    uint8_t devsel_timing            : 2;
    uint8_t signaled_target_abort    : 1;
    uint8_t received_target_abort    : 1;
    uint8_t received_master_abort    : 1;
    uint8_t signaled_system_error    : 1;
    uint8_t detected_parity_error    : 1;
} __attribute__((packed)) pci_status_register_t;

/**
 * @struct pci_header_type_register_t
 * @brief pci device type data inside pci_common_header_t
 */
typedef struct pci_header_type_register_t {
    uint8_t header_type   : 7; ///< device types: 0x00 generic, 0x01 bridge, 0x02 cardbus
    uint8_t multifunction : 1; ///< if 1 then device is multi-function device and has 8 functions, else only function 0.
} __attribute__((packed)) pci_header_type_register_t; ///< short hand for struct

typedef struct pci_bist_register_t {
    uint8_t completion_code : 4;
    uint8_t reserved0       : 2;
    uint8_t start_bist      : 1;
    uint8_t bist_capable    : 1;
} __attribute__((packed)) pci_bist_register_t;


typedef union pci_bar_register_t {
    struct bar_type_t {
        uint32_t type      : 1;
        uint32_t reserved0 : 31;
    } __attribute__((packed)) bar_type;
    struct memory_space_bar_t {
        uint32_t bar_type     : 1;
        uint32_t type         : 2;
        uint32_t prefetchable : 1;
        uint32_t base_address : 28;
    } __attribute__((packed)) memory_space_bar;
    struct io_space_bar_t {
        uint32_t bar_type     : 1;
        uint32_t reserved0    : 1;
        uint32_t base_address : 30;
    } __attribute__((packed)) io_space_bar;
} __attribute__((packed)) pci_bar_register_t;

typedef struct pci_common_header_t {
    uint16_t                   vendor_id : 16;
    uint16_t                   device_id : 16;
    pci_command_register_t     command;
    pci_status_register_t      status;
    uint8_t                    revsionid       : 8;
    uint8_t                    prog_if         : 8;
    uint8_t                    subclass_code   : 8;
    uint8_t                    class_code      : 8;
    uint8_t                    cache_line_size : 8;
    uint8_t                    latency_timer   : 8;
    pci_header_type_register_t header_type;
    pci_bist_register_t        bist;
} __attribute__((packed, aligned(4))) pci_common_header_t;

typedef struct pci_generic_device_t {
    pci_common_header_t common_header;
    pci_bar_register_t  bar0;
    pci_bar_register_t  bar1;
    pci_bar_register_t  bar2;
    pci_bar_register_t  bar3;
    pci_bar_register_t  bar4;
    pci_bar_register_t  bar5;
    uint32_t            cardbus_cis_pointer        : 32;
    uint16_t            subsystem_vendor_id        : 16;
    uint16_t            subsystem_id               : 16;
    uint32_t            expension_rom_base_address : 32;
    uint8_t             capabilities_pointer       : 8;
    uint32_t            reserved0                  : 24;
    uint32_t            reserved1                  : 32;
    uint8_t             interrupt_line             : 8;
    uint8_t             interrupt_pin              : 8;
    uint8_t             min_grant                  : 8;
    uint8_t             max_latency                : 8;
} __attribute__((packed, aligned(4))) pci_generic_device_t;


typedef struct pci_pci2pci_bridge_t {
    pci_common_header_t   common_header;
    pci_bar_register_t    bar0;
    pci_bar_register_t    bar1;
    uint8_t               primary_bus_number      : 8;
    uint8_t               secondary_bus_number    : 8;
    uint8_t               subordinate_bus_number  : 8;
    uint8_t               secondary_latency_timer : 8;
    uint8_t               io_base                 : 8;
    uint8_t               io_limit                : 8;
    pci_status_register_t secondary_status;
    uint16_t              memory_base                     : 16;
    uint16_t              memory_limit                    : 16;
    uint16_t              prefetchable_memory_base        : 16;
    uint16_t              prefetchable_memory_limit       : 16;
    uint32_t              prefetchable_base_upper_32bits  : 32;
    uint32_t              prefetchable_limit_upper_32bits : 32;
    uint16_t              io_base_upper_16bits            : 16;
    uint16_t              io_limit_upper_16bits           : 16;
    uint8_t               capabilities_pointer            : 8;
    uint32_t              reserved0                       : 24;
    uint32_t              expension_rom_base_address      : 32;
    uint8_t               interrupt_line                  : 8;
    uint8_t               interrupt_pin                   : 8;
    uint16_t              bridge_control                  : 16;
} __attribute__((packed, aligned(4))) pci_pci2pci_bridge_t;


typedef struct pci_cardbus_bridge_t {
    pci_common_header_t   common_header;
    uint32_t              cardbus_socket_or_exca_base_address : 32;
    uint8_t               capabilities_pointer                : 8;
    uint8_t               reserved0                           : 8;
    pci_status_register_t secondary_status;
    uint8_t               pci_bus_number                        : 8;
    uint8_t               cardbus_bus_number                    : 8;
    uint8_t               subordinate_bus_number                : 8;
    uint8_t               cardbus_latency_timer                 : 8;
    uint32_t              memory_base_address0                  : 32;
    uint32_t              memory_limit0                         : 32;
    uint32_t              memory_base_address1                  : 32;
    uint32_t              memory_limit1                         : 32;
    uint32_t              io_base_address0                      : 32;
    uint32_t              io_limit0                             : 32;
    uint32_t              io_base_address1                      : 32;
    uint32_t              io_limit1                             : 32;
    uint8_t               interrupt_line                        : 8;
    uint8_t               interrupt_pin                         : 8;
    uint16_t              bridge_control                        : 16;
    uint16_t              subsystem_vendor_id                   : 16;
    uint16_t              subsystem_device_id                   : 16;
    uint32_t              pccard_16bit_legacy_mode_base_address : 32;
} __attribute__((packed, aligned(4))) pci_cardbus_bridge_t;

/**
 * @struct pci_dev_t
 * @brief the pci device info returned by the iterator
 */
typedef struct pci_dev_t {
    uint16_t             group_number; ///< device's bus group number.
    uint8_t              bus_number; ///< bus number of the device
    uint8_t              device_number; ///< device number
    uint8_t              function_number; ///< device function number
    uint64_t             header_size; ///< header size of the device
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

typedef struct pci_capability_t {
    uint8_t capability_id : 8;
    uint8_t next_pointer  : 8;
} __attribute__((packed)) pci_capability_t;

typedef struct pci_capability_msi_t {
    pci_capability_t cap;
    uint8_t          enable                  : 1;
    uint8_t          multiple_message_count  : 3;
    uint8_t          multiple_message_enable : 3;
    uint8_t          ma64_support            : 1;
    uint8_t          per_vector_masking      : 1;
    uint8_t          reserved0               : 7;
    union {
        struct ma32_t {
            uint32_t message_address;
            uint16_t message_data;
            uint16_t reserved0;
            uint32_t mask;
            uint32_t pending;
        } ma32;
        struct ma64_t {
            uint64_t message_address;
            uint16_t message_data;
            uint16_t reserved0;
            uint32_t mask;
            uint32_t pending;
        } ma64;
    };
} __attribute__((packed)) pci_capability_msi_t;

typedef struct pci_capability_msix_t {
    pci_capability_t cap;
    uint16_t         table_size         : 11;
    uint16_t         reserved           : 3;
    uint16_t         function_mask      : 1;
    uint16_t         enable             : 1;
    uint32_t         bir                : 3;
    uint32_t         table_offset       : 29;
    uint32_t         pending_bit_bir    : 3;
    uint32_t         pending_bit_offset : 29;
}__attribute__((packed)) pci_capability_msix_t;

typedef struct pci_capability_msix_table_entry_t {
    uint64_t message_address;
    uint32_t message_data;
    uint32_t masked;
}__attribute__((packed)) pci_capability_msix_table_entry_t;

typedef struct pci_capability_msix_table_t {
    pci_capability_msix_table_entry_t entries[0];
}__attribute__((packed)) pci_capability_msix_table_t;

typedef struct pci_context_t {
    memory_heap_t* heap;
    list_t*        sata_controllers;
    list_t*        nvme_controllers;
    list_t*        network_controllers;
    list_t*        display_controllers;
    list_t*        usb_controllers;
    list_t*        input_controllers;
    list_t*        other_devices;
} pci_context_t;

pci_context_t* pci_get_context(void);
void           pci_set_context(pci_context_t* pci_context);

uint64_t  pci_get_bar_size(pci_generic_device_t* pci_dev, uint8_t bar_no);
uint64_t  pci_get_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no);
int8_t    pci_set_bar_address(pci_generic_device_t* pci_dev, uint8_t bar_no, uint64_t bar_fa);
int8_t    pci_msix_configure(pci_generic_device_t* pci_gen_dev, pci_capability_msix_t* msix_cap);
uint8_t   pci_msix_set_isr(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector, interrupt_irq isr);
uint8_t   pci_msix_update_lapic(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector);
boolean_t pci_msix_is_pending_bit_set(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector);
int8_t    pci_msix_clear_pending_bit(pci_generic_device_t* pci_dev, pci_capability_msix_t* msix_cap, uint16_t msix_vector);

void pci_disable_interrupt(pci_generic_device_t* pci_dev);
void pci_enable_interrupt(pci_generic_device_t* pci_dev);

int8_t pci_setup(memory_heap_t* heap);

const pci_dev_t* pci_find_device_by_address(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number);

#ifdef __cplusplus
}
#endif

#endif
