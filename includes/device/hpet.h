/**
 * @file hpet.h
 * @brief High Precision Event Timer (HPET) support
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___HPET_H
/*! macro for avoiding multiple inclusion */
#define ___HPET_H 0

#include <types.h>
#include <acpi.h>
#include <utils.h>

/**
 * @struct hpet_address_t
 * @brief  HPET address structure
 */
typedef struct hpet_address_t {
    uint8_t  address_space_id; ///< 0 - system memory, 1 - system I/O
    uint8_t  register_bit_width; ///< 8, 16, 32, 64
    uint8_t  register_bit_offset; ///< 0 - 7, 0 - 15, 0 - 31, 0 - 63
    uint8_t  reserved; ///< must be 0
    uint64_t address; ///< address of HPET
}__attribute__((packed)) hpet_address_t; ///< HPET address structure

/**
 * @struct hpet_table_t
 * @brief  ACPI HPET table structure
 */
typedef struct hpet_table_t {
    acpi_sdt_header_t header; ///< ACPI SDT header
    uint8_t           hardware_rev_id; ///< hardware revision ID
    uint8_t           comparator_count   : 5; ///< number of comparators
    uint8_t           counter_size       : 1; ///< 0 - 32 bit, 1 - 64 bit
    uint8_t           reserved           : 1; ///< must be 0
    uint8_t           legacy_replacement : 1; ///< 0 - no, 1 - yes
    uint16_t          pci_vendor_id; ///< PCI vendor ID
    hpet_address_t    address; ///< HPET address
    uint8_t           hpet_number; ///< HPET number
    uint16_t          minimum_tick; ///< minimum tick in femtoseconds
    uint8_t           page_protection; ///< page protection
}__attribute__((packed)) hpet_table_t; ///< ACPI HPET table structure

/**
 * @union hpet_capabilities_t
 * @brief  HPET capabilities register
 */
typedef  union hpet_capabilities_t {
    struct {
        uint64_t rev_id             : 8; ///< revision ID
        uint64_t number_of_timers   : 5; ///< number of comparators
        uint64_t count_size_cap     : 1; ///< 0 - 32 bit, 1 - 64 bit
        uint64_t reserved           : 1; ///< must be 0
        uint64_t legacy_replacement : 1; ///< 0 - no, 1 - yes
        uint64_t vendor_id          : 16; ///< PCI vendor ID
        uint64_t counter_clk_period : 32; ///< counter clock period in femtoseconds
    } __attribute__((packed)) fields; ///< HPET capabilities register fields
    uint64_t raw; ///< raw HPET capabilities register
} __attribute__((packed)) hpet_capabilities_t; ///< HPET capabilities register

/**
 * @struct hpet_congfiguration_t
 * @brief  HPET configuration register
 */
typedef  struct hpet_congfiguration_t {
    uint64_t enable_cnf : 1; ///< enable configuration
    uint64_t leg_rt_cnf : 1; ///< legacy replacement configuration
    uint64_t reserved   : 62; ///< must be 0
} __attribute__((packed)) hpet_configuration_t; ///< HPET configuration register

/**
 * @struct hpet_interrupt_status_t
 * @brief  HPET interrupt status register
 */
typedef  struct hpet_interrupt_status_t {
    uint64_t status   : 32; ///< interrupt status
    uint64_t reserved : 32; ///< must be 0
} __attribute__((packed)) hpet_interrupt_status_t; ///< HPET interrupt status register

/*! macro for getting interrupt status of timer i of hpet x */
#define HPET_INTERRUPT_STATUS_OF(x, i) ((x->interrupt_status.status >> i ) & 1)

/**
 * @union hpet_timer_configuration_t
 * @brief  HPET timer configuration register
 */
typedef union hpet_timer_configuration_t {
    struct {
        uint64_t reserved0                 : 1; ///< must be 0
        uint64_t interrupt_type            : 1; ///< 0 - edge, 1 - level
        uint64_t interrupt_enable          : 1; ///< 0 - no, 1 - yes
        uint64_t timer_type                : 1; ///< 0 - one shot, 1 - periodic
        uint64_t periodic_capable          : 1; ///< 0 - no, 1 - yes
        uint64_t size_capable              : 1; ///< 0 - 32 bit, 1 - 64 bit
        uint64_t value_set                 : 1; ///< 0 - no, 1 - yes
        uint64_t reserved1                 : 1; ///< must be 0
        uint64_t force_32bit               : 1; ///< 0 - no, 1 - yes
        uint64_t interrupt_route           : 5; ///< interrupt route
        uint64_t fsb_enable                : 1; ///< 0 - no, 1 - yes
        uint64_t fsb_interrupt_enable      : 1; ///< 0 - no, 1 - yes
        uint64_t reserved2                 : 16; ///< must be 0
        uint64_t interrup_route_capability : 32; ///< interrupt route capability, which bits are set, that interrupt route is supported
    } __attribute__((packed)) fields; ///< HPET timer configuration register fields
    uint64_t raw; ///< raw HPET timer configuration register
} __attribute__((packed)) hpet_timer_configuration_t; ///< HPET timer configuration register

/**
 * @struct hpet_t
 * @brief  HPET structure
 */
typedef  struct hpet_t {
    volatile uint64_t capabilities; ///< HPET capabilities register @see hpet_capabilities_t
    uint64_t          reserved0; ///< must be 0
    volatile uint64_t configuration; ///< HPET configuration register @see hpet_configuration_t
    uint64_t          reserved1; ///< must be 0
    uint64_t          interrupt_status; ///< HPET interrupt status register @see hpet_interrupt_status_t
    uint64_t          reserved2[25]; ///< must be 0
    volatile uint64_t main_counter; ///< main counter
    uint64_t          reserved3; ///< must be 0
    volatile uint64_t timer0_configuration; ///< HPET timer 0 configuration register @see hpet_timer_configuration_t
    volatile uint64_t timer0_comparator_value; ///< HPET timer 0 comparator value
    volatile uint64_t timer0_fsb_interrupt_route; ///< HPET timer 0 FSB interrupt route
    uint64_t          timer0_reserved; ///< must be 0
    volatile uint64_t timer1_configuration; ///< HPET timer 1 configuration register @see hpet_timer_configuration_t
    volatile uint64_t timer1_comparator_value; ///< HPET timer 1 comparator value
    volatile uint64_t timer1_fsb_interrupt_route; ///< HPET timer 1 FSB interrupt route
    uint64_t          timer1_reserved; ///< must be 0
    volatile uint64_t timer2_configuration; ///< HPET timer 2 configuration register @see hpet_timer_configuration_t
    volatile uint64_t timer2_comparator_value; ///< HPET timer 2 comparator value
    volatile uint64_t timer2_fsb_interrupt_route; ///< HPET timer 2 FSB interrupt route
    uint64_t          timer2_reserved; ///< must be 0
} __attribute__((packed)) hpet_t; ///< HPET structure

/*! variable for hpet enabled flag */
extern boolean_t hpet_enabled;

/**
 * @brief initializes hpet
 * @return 0 if hpet was initialized, -1 otherwise
 */
int8_t hpet_init(void);

#endif
