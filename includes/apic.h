/**
 * @file apic.h
 * @brief apic and ioapic interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___APIC_H
/*! prevent duplicate header error macro */
#define ___APIC_H 0

#include <types.h>
#include <linkedlist.h>
#include <acpi.h>

#define APIC_MSR_ADDRESS        0x1B
#define APIC_MSR_ENABLE_APIC    0x800UL
#define APIC_MSR_ENABLE_X2APIC  0x400UL

#define APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT    0xF0
#define APIC_REGISTER_OFFSET_EOI                   0xB0
#define APIC_REGISTER_OFFSET_TIMER_LVT             0x320
#define APIC_REGISTER_OFFSET_TIMER_INITIAL_VALUE   0x380
#define APIC_REGISTER_OFFSET_TIMER_CURRENT_VALUE   0x390
#define APIC_REGISTER_OFFSET_TIMER_DIVIDER         0x3E0

#define APIC_IOAPIC_REGISTER_IDENTIFICATION 0x00
#define APIC_IOAPIC_REGISTER_VERSION        0x01
#define APIC_IOAPIC_REGISTER_ARBITRATION    0x02
#define APIC_IOAPIC_REGISTER_IRQ_BASE       0x10

#define APIC_IOAPIC_DELIVERY_MODE_FIXED            (0 << 8)
#define APIC_IOAPIC_DELIVERY_MODE_LOWEST_PRIORITY  (1 << 8)
#define APIC_IOAPIC_DELIVERY_MODE_SMI              (2 << 8)
#define APIC_IOAPIC_DELIVERY_MODE_NMI              (4 << 8)
#define APIC_IOAPIC_DELIVERY_MODE_INIT             (5 << 8)
#define APIC_IOAPIC_DELIVERY_MODE_EXTERNAL_INT     (7 << 8)
#define APIC_IOAPIC_DESTINATION_MODE_PHYSICAL      (0 << 11)
#define APIC_IOAPIC_DESTINATION_MODE_LOGICAL       (1 << 11)
#define APIC_IOAPIC_DELIVERY_STATUS_RELAX          (0 << 12)
#define APIC_IOAPIC_DELIVERY_STATUS_WAITING        (1 << 12)
#define APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH       (0 << 13)
#define APIC_IOAPIC_PIN_POLARITY_ACTIVE_LOW        (1 << 13)
#define APIC_IOAPIC_TRIGGER_MODE_EDGE              (0 << 15)
#define APIC_IOAPIC_TRIGGER_MODE_LEVEL             (1 << 15)
#define APIC_IOAPIC_INTERRUPT_ENABLED              (0 << 16)
#define APIC_IOAPIC_INTERRUPT_DISABLED             (1 << 16)

#define APIC_INTERRUPT_ENABLED  APIC_IOAPIC_INTERRUPT_ENABLED
#define APIC_INTERRUPT_DISABLED APIC_IOAPIC_INTERRUPT_DISABLED

#define APIC_TIMER_ONESHOT          (0 << 17)
#define APIC_TIMER_PERIODIC         (1 << 17)
#define APIC_TIMER_TSC_DEADLINE     (2 << 17)


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

int8_t apic_setup(acpi_xrsdp_descriptor_t* desc);

int8_t apic_init_apic(linkedlist_t apic_entries);

int8_t apic_ioapic_setup_irq(uint8_t irq, uint32_t props);

int8_t apic_ioapic_switch_irq(uint8_t irq, uint32_t disabled);
#define apic_ioapic_enable_irq(irq) apic_ioapic_switch_irq(irq, 0)
#define apic_ioapic_disable_irq(irq) apic_ioapic_switch_irq(irq, 1)

uint8_t apic_get_irq_override(uint8_t old_irq);

void  apic_eoi();

#endif
