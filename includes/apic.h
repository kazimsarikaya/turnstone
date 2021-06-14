/**
 * @file apic.h
 * @brief apic and ioapic interface
 */
#ifndef ___APIC_H
/*! prevent duplicate header error macro */
#define ___APIC_H 0

#include <types.h>
#include <linkedlist.h>

#define APIC_MSR_ADDRESS 0x1B
#define APIC_MSR_ENABLE_APIC 0x800
#define APIC_BASE_IRQ_INT_MAP 0x20

#define APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT 0xF0

#define APIC_IOAPIC_REGISTER_IDENTIFICATION 0x00
#define APIC_IOAPIC_REGISTER_VERSION        0x01
#define APIC_IOAPIC_REGISTER_ARBITRATION    0x02
#define APIC_IOAPIC_REGISTER_IRQ_BASE       0x10
#define APIC_IOAPIC_INTERRUPT_DISABLED      (1 << 16)
#define APIC_IOAPIC_MAX_REDIRECTION_ENTRY(r)  (((r >> 16) & 0xFF) + 1)

typedef struct {
	uint32_t selector;
	uint32_t reserved0[3];
	uint32_t value;
}__attribute__((packed)) apic_ioapic_register_t;

typedef struct {
	uint8_t vector : 8;
	uint8_t apic_software_enable : 1;
	uint8_t focus_cpu_core_checking : 1;
	uint32_t reserved0 : 22;
}__attribute__((packed)) apic_register_spurious_interrupt_t;

int8_t apic_init_apic(linkedlist_t apic_entries);

#endif
