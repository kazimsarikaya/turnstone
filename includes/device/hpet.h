/**
 * @file hpet.h
 * @brief High Precision Event Timer (HPET) support
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef __HPET_H
#define __HPET_H 0

#include <types.h>
#include <acpi.h>
#include <utils.h>


typedef struct hpet_address_t {
    uint8_t  address_space_id;
    uint8_t  register_bit_width;
    uint8_t  register_bit_offset;
    uint8_t  reserved;
    uint64_t address;
}__attribute__((packed)) hpet_address_t;

typedef struct hpet_table_t {
    acpi_sdt_header_t header;
    uint8_t           hardware_rev_id;
    uint8_t           comparator_count   : 5;
    uint8_t           counter_size       : 1;
    uint8_t           reserved           : 1;
    uint8_t           legacy_replacement : 1;
    uint16_t          pci_vendor_id;
    hpet_address_t    address;
    uint8_t           hpet_number;
    uint16_t          minimum_tick;
    uint8_t           page_protection;
}__attribute__((packed)) hpet_table_t;

typedef  union hpet_capabilities_t {
    struct {
        uint64_t rev_id             : 8;
        uint64_t number_of_timers   : 5;
        uint64_t count_size_cap     : 1;
        uint64_t reserved           : 1;
        uint64_t legacy_replacement : 1;
        uint64_t vendor_id          : 16;
        uint64_t counter_clk_period : 32;
    } __attribute__((packed)) fields;
    uint64_t raw;
} __attribute__((packed)) hpet_capabilities_t;

typedef  struct hpet_congfiguration_t {
    uint64_t enable_cnf : 1;
    uint64_t leg_rt_cnf : 1;
    uint64_t reserved   : 62;
} __attribute__((packed)) hpet_configuration_t;

typedef  struct hpet_interrupt_status_t {
    uint64_t status   : 32;
    uint64_t reserved : 32;
} __attribute__((packed)) hpet_interrupt_status_t;

#define HPET_INTERRUPT_STATUS_OF(x, i) ((x->interrupt_status.status >> i ) & 1)


typedef union hpet_timer_configuration_t {
    struct {
        uint64_t reserved0                 : 1;
        uint64_t interrupt_type            : 1;
        uint64_t interrupt_enable          : 1;
        uint64_t timer_type                : 1;
        uint64_t periodic_capable          : 1;
        uint64_t size_capable              : 1;
        uint64_t value_set                 : 1;
        uint64_t reserved1                 : 1;
        uint64_t force_32bit               : 1;
        uint64_t interrupt_route           : 5;
        uint64_t fsb_enable                : 1;
        uint64_t fsb_interrupt_enable      : 1;
        uint64_t reserved2                 : 16;
        uint64_t interrup_route_capability : 32;
    } __attribute__((packed)) fields;
    uint64_t raw;
} __attribute__((packed)) hpet_timer_configuration_t;

typedef  struct hpet_t {
    volatile uint64_t capabilities;
    uint64_t          reserved0;
    volatile uint64_t configuration;
    uint64_t          reserved1;
    uint64_t          interrupt_status;
    uint64_t          reserved2[25];
    volatile uint64_t main_counter;
    uint64_t          reserved3;
    volatile uint64_t timer0_configuration;
    volatile uint64_t timer0_comparator_value;
    volatile uint64_t timer0_fsb_interrupt_route;
    uint64_t          timer0_reserved;
    volatile uint64_t timer1_configuration;
    volatile uint64_t timer1_comparator_value;
    volatile uint64_t timer1_fsb_interrupt_route;
    uint64_t          timer1_reserved;
    volatile uint64_t timer2_configuration;
    volatile uint64_t timer2_comparator_value;
    volatile uint64_t timer2_fsb_interrupt_route;
    uint64_t          timer2_reserved;
} __attribute__((packed)) hpet_t;

extern boolean_t hpet_enabled;

int8_t hpet_init(void);

#endif
