/**
 * @file apic.h
 * @brief Advanced Programmable Interrupt Controller (APIC) and I/O APIC interface.
 *
 * This header defines structures, macros, and function prototypes for interacting
 * with the APIC and I/O APIC hardware. It provides functionalities for interrupt
 * handling, configuration, and inter-processor interrupts (IPIs).
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___APIC_H
/*! @brief Prevent duplicate header inclusion. */
#define ___APIC_H 0

#include <types.h>
#include <list.h>
#include <acpi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name APIC MSR Addresses
 * @brief Machine Specific Register (MSR) addresses for APIC.
 */
/**@{*/
#define APIC_MSR_ADDRESS        0x1B /**< @brief APIC Base Address MSR. */
#define APIC_MSR_ENABLE_APIC    0x800UL /**< @brief Enable APIC bit in APIC Base Address MSR. */
#define APIC_MSR_ENABLE_X2APIC  0x400UL /**< @brief Enable x2APIC mode bit in APIC Base Address MSR. */
/**@}*/

/**
 * @name APIC Local Register Offsets (Physical Addressing)
 * @brief Offsets for accessing Local APIC registers in physical mode.
 */
/**@{*/
#define APIC_REGISTER_OFFSET_ID                    0x20 /**< @brief Local APIC ID Register. */
#define APIC_REGISTER_OFFSET_VERSION               0x30 /**< @brief Local APIC Version Register. */
#define APIC_REGISTER_OFFSET_DFR                   0xE0 /**< @brief Local APIC Destination Format Register. */
#define APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT    0xF0 /**< @brief Local APIC Spurious Interrupt Vector Register. */
#define APIC_REGISTER_OFFSET_EOI                   0xB0 /**< @brief Local APIC End-Of-Interrupt Register. */
#define APIC_REGISTER_OFFSET_ISR0                  0x100 /**< @brief Local APIC Interrupt Service Register 0. */
#define APIC_REGISTER_OFFSET_TIMER_LVT             0x320 /**< @brief Local APIC Timer Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_TERMAL_SENSOR_LVT     0x330 /**< @brief Local APIC Thermal Sensor Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_PERF_COUNTER_LVT      0x340 /**< @brief Local APIC Performance Counter Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_LINT0_LVT             0x350 /**< @brief Local APIC LINT0 Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_LINT1_LVT             0x360 /**< @brief Local APIC LINT1 Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_ERROR_LVT             0x370 /**< @brief Local APIC Error Local Vector Table Entry. */
#define APIC_REGISTER_OFFSET_TIMER_INITIAL_VALUE   0x380 /**< @brief Local APIC Timer Initial Count Register. */
#define APIC_REGISTER_OFFSET_TIMER_CURRENT_VALUE   0x390 /**< @brief Local APIC Timer Current Count Register. */
#define APIC_REGISTER_OFFSET_TIMER_DIVIDER         0x3E0 /**< @brief Local APIC Timer Divide Configuration Register. */

#define APIC_REGISTER_OFFSET_ISR0                  0x100 /**< @brief Duplicate definition for ISR0. */
#define APIC_REGISTER_OFFSET_ISR1                  0x110 /**< @brief Local APIC Interrupt Service Register 1. */
#define APIC_REGISTER_OFFSET_ISR2                  0x120 /**< @brief Local APIC Interrupt Service Register 2. */
#define APIC_REGISTER_OFFSET_ISR3                  0x130 /**< @brief Local APIC Interrupt Service Register 3. */
#define APIC_REGISTER_OFFSET_ISR4                  0x140 /**< @brief Local APIC Interrupt Service Register 4. */
#define APIC_REGISTER_OFFSET_ISR5                  0x150 /**< @brief Local APIC Interrupt Service Register 5. */
#define APIC_REGISTER_OFFSET_ISR6                  0x160 /**< @brief Local APIC Interrupt Service Register 6. */
#define APIC_REGISTER_OFFSET_ISR7                  0x170 /**< @brief Local APIC Interrupt Service Register 7. */

#define APIC_REGISTER_OFFSET_IRR0                  0x200 /**< @brief Local APIC Interrupt Request Register 0. */
#define APIC_REGISTER_OFFSET_IRR1                  0x210 /**< @brief Local APIC Interrupt Request Register 1. */
#define APIC_REGISTER_OFFSET_IRR2                  0x220 /**< @brief Local APIC Interrupt Request Register 2. */
#define APIC_REGISTER_OFFSET_IRR3                  0x230 /**< @brief Local APIC Interrupt Request Register 3. */
#define APIC_REGISTER_OFFSET_IRR4                  0x240 /**< @brief Local APIC Interrupt Request Register 4. */
#define APIC_REGISTER_OFFSET_IRR5                  0x250 /**< @brief Local APIC Interrupt Request Register 5. */
#define APIC_REGISTER_OFFSET_IRR6                  0x260 /**< @brief Local APIC Interrupt Request Register 6. */
#define APIC_REGISTER_OFFSET_IRR7                  0x270 /**< @brief Local APIC Interrupt Request Register 7. */

#define APIC_REGISTER_OFFSET_ICR_LOW               0x300 /**< @brief Local APIC Interrupt Command Register (Low Dword). */
#define APIC_REGISTER_OFFSET_ICR_HIGH              0x310 /**< @brief Local APIC Interrupt Command Register (High Dword). */
/**@}*/

/**
 * @name APIC x2APIC MSR Addresses
 * @brief MSR addresses for accessing APIC registers in x2APIC mode.
 */
/**@{*/
#define APIC_X2APIC_MSR_APICID                      0x802 /**< @brief x2APIC APIC ID Register. */
#define APIC_X2APIC_MSR_VERSION                     0x803 /**< @brief x2APIC Version Register. */
#define APIC_X2APIC_MSR_TPR                         0x808 /**< @brief x2APIC Task Priority Register. */
#define APIC_X2APIC_MSR_PPR                         0x80A /**< @brief x2APIC Processor Priority Register. */
#define APIC_X2APIC_MSR_EOI                         0x80B /**< @brief x2APIC End-Of-Interrupt Register. */
#define APIC_X2APIC_MSR_LDR                         0x80D /**< @brief x2APIC Logical Destination Register. */
#define APIC_X2APIC_MSR_SIVR                        0x80F /**< @brief x2APIC Spurious Interrupt Vector Register. */
#define APIC_X2APIC_MSR_ISR0                        0x810 /**< @brief x2APIC Interrupt Service Register 0. */
#define APIC_X2APIC_MSR_ISR1                        0x811 /**< @brief x2APIC Interrupt Service Register 1. */
#define APIC_X2APIC_MSR_ISR2                        0x812 /**< @brief x2APIC Interrupt Service Register 2. */
#define APIC_X2APIC_MSR_ISR3                        0x813 /**< @brief x2APIC Interrupt Service Register 3. */
#define APIC_X2APIC_MSR_ISR4                        0x814 /**< @brief x2APIC Interrupt Service Register 4. */
#define APIC_X2APIC_MSR_ISR5                        0x815 /**< @brief x2APIC Interrupt Service Register 5. */
#define APIC_X2APIC_MSR_ISR6                        0x816 /**< @brief x2APIC Interrupt Service Register 6. */
#define APIC_X2APIC_MSR_ISR7                        0x817 /**< @brief x2APIC Interrupt Service Register 7. */
#define APIC_X2APIC_MSR_TMR0                        0x818 /**< @brief x2APIC Timer Register 0. */
#define APIC_X2APIC_MSR_TMR1                        0x819 /**< @brief x2APIC Timer Register 1. */
#define APIC_X2APIC_MSR_TMR2                        0x81A /**< @brief x2APIC Timer Register 2. */
#define APIC_X2APIC_MSR_TMR3                        0x81B /**< @brief x2APIC Timer Register 3. */
#define APIC_X2APIC_MSR_TMR4                        0x81C /**< @brief x2APIC Timer Register 4. */
#define APIC_X2APIC_MSR_TMR5                        0x81D /**< @brief x2APIC Timer Register 5. */
#define APIC_X2APIC_MSR_TMR6                        0x81E /**< @brief x2APIC Timer Register 6. */
#define APIC_X2APIC_MSR_TMR7                        0x81F /**< @brief x2APIC Timer Register 7. */
#define APIC_X2APIC_MSR_IRR0                        0x820 /**< @brief x2APIC Interrupt Request Register 0. */
#define APIC_X2APIC_MSR_IRR1                        0x821 /**< @brief x2APIC Interrupt Request Register 1. */
#define APIC_X2APIC_MSR_IRR2                        0x822 /**< @brief x2APIC Interrupt Request Register 2. */
#define APIC_X2APIC_MSR_IRR3                        0x823 /**< @brief x2APIC Interrupt Request Register 3. */
#define APIC_X2APIC_MSR_IRR4                        0x824 /**< @brief x2APIC Interrupt Request Register 4. */
#define APIC_X2APIC_MSR_IRR5                        0x825 /**< @brief x2APIC Interrupt Request Register 5. */
#define APIC_X2APIC_MSR_IRR6                        0x826 /**< @brief x2APIC Interrupt Request Register 6. */
#define APIC_X2APIC_MSR_IRR7                        0x827 /**< @brief x2APIC Interrupt Request Register 7. */
#define APIC_X2APIC_MSR_ESR                         0x828 /**< @brief x2APIC Error Status Register. */
#define APIC_X2APIC_MSR_LVT_CMCI                    0x82F /**< @brief x2APIC Corrected Machine Check Interrupt Local Vector Table Entry. */
#define APIC_X2APIC_MSR_ICR                         0x830 /**< @brief x2APIC Interrupt Command Register. */
#define APIC_X2APIC_MSR_LVT_TIMER                   0x832 /**< @brief x2APIC Timer Local Vector Table Entry. */
#define APIC_X2APIC_MSR_LVT_THERMAL                 0x833 /**< @brief x2APIC Thermal Sensor Local Vector Table Entry. */
#define APIC_X2APIC_MSR_LVT_PMI                     0x834 /**< @brief x2APIC Performance Monitoring Interrupt Local Vector Table Entry. */
#define APIC_X2APIC_MSR_LVT_LINT0                   0x835 /**< @brief x2APIC LINT0 Local Vector Table Entry. */
#define APIC_X2APIC_MSR_LVT_LINT1                   0x836 /**< @brief x2APIC LINT1 Local Vector Table Entry. */
#define APIC_X2APIC_MSR_LVT_ERROR                   0x837 /**< @brief x2APIC Error Local Vector Table Entry. */
#define APIC_X2APIC_MSR_TIMER_INITIAL_VALUE         0x838 /**< @brief x2APIC Timer Initial Count Register. */
#define APIC_X2APIC_MSR_TIMER_CURRENT_VALUE         0x839 /**< @brief x2APIC Timer Current Count Register. */
#define APIC_X2APIC_MSR_TIMER_DIVIDER               0x83E /**< @brief x2APIC Timer Divide Configuration Register. */
#define APIC_X2APIC_MSR_SELF_IPI                    0x83F /**< @brief x2APIC Self-IPI Register. */
#define APIC_X2APIC_MSR_EXTENDED_APIC_CONTROL       0x841 /**< @brief x2APIC Extended APIC Control Register. */
#define APIC_X2APIC_MSR_SEOI                        0x842 /**< @brief x2APIC Software End-Of-Interrupt Register. */
/**@}*/

/**
 * @name Interrupt Command Register (ICR) Fields
 * @brief Bit fields for configuring the Interrupt Command Register.
 */
/**@{*/
#define APIC_ICR_DESTINATION_MODE_PHYSICAL          (0 << 11) /**< @brief Destination mode: Physical. */
#define APIC_ICR_DESTINATION_MODE_LOGICAL           (1 << 11) /**< @brief Destination mode: Logical. */
#define APIC_ICR_DELIVERY_MODE_FIXED                (0 << 8) /**< @brief Delivery mode: Fixed. */
#define APIC_ICR_DELIVERY_MODE_LOWEST_PRIORITY      (1 << 8) /**< @brief Delivery mode: Lowest Priority. */
#define APIC_ICR_DELIVERY_MODE_SMI                  (2 << 8) /**< @brief Delivery mode: SMI. */
#define APIC_ICR_DELIVERY_MODE_NMI                  (4 << 8) /**< @brief Delivery mode: NMI. */
#define APIC_ICR_DELIVERY_MODE_INIT                 (5 << 8) /**< @brief Delivery mode: INIT. */
#define APIC_ICR_DELIVERY_MODE_STARTUP              (6 << 8) /**< @brief Delivery mode: Startup. */
#define APIC_ICR_DELIVERY_MODE_EXTERNAL_INT         (7 << 8) /**< @brief Delivery mode: External Interrupt. */
#define APIC_ICR_DELIVERY_STATUS_IDLE               (0 << 12) /**< @brief Delivery status: Idle. */
#define APIC_ICR_DELIVERY_STATUS_SEND_PENDING       (1 << 12) /**< @brief Delivery status: Send Pending. */
#define APIC_ICR_LEVEL_ASSERT                       (1 << 14) /**< @brief Level assertion: Assert. */
#define APIC_ICR_LEVEL_DEASSERT                     (0 << 14) /**< @brief Level assertion: Deassert. */
#define APIC_ICR_TRIGGER_MODE_EDGE                  (0 << 15) /**< @brief Trigger mode: Edge. */
#define APIC_ICR_TRIGGER_MODE_LEVEL                 (1 << 15) /**< @brief Trigger mode: Level. */
#define APIC_ICR_DESTINATION_SHORTHAND_NONE         (0 << 18) /**< @brief Destination shorthand: None. */
#define APIC_ICR_DESTINATION_SHORTHAND_SELF         (1 << 18) /**< @brief Destination shorthand: Self. */
#define APIC_ICR_DESTINATION_SHORTHAND_ALL          (2 << 18) /**< @brief Destination shorthand: All. */
#define APIC_ICR_DESTINATION_SHORTHAND_ALL_BUT_SELF (3 << 18) /**< @brief Destination shorthand: All but Self. */
#define APIC_ICR_DESTINATION_SHIFT                        24 /**< @brief Shift for the destination field in ICR. */
/**@}*/

/**
 * @name IOAPIC Register Offsets
 * @brief Offsets for accessing I/O APIC registers.
 */
/**@{*/
#define APIC_IOAPIC_REGISTER_IDENTIFICATION 0x00 /**< @brief IOAPIC Identification Register. */
#define APIC_IOAPIC_REGISTER_VERSION        0x01 /**< @brief IOAPIC Version Register. */
#define APIC_IOAPIC_REGISTER_ARBITRATION    0x02 /**< @brief IOAPIC Arbitration Register. */
#define APIC_IOAPIC_REGISTER_IRQ_BASE       0x10 /**< @brief Base offset for IOAPIC IRQ Redirection Table Entries. */
/**@}*/

/**
 * @name IOAPIC Redirection Entry Fields
 * @brief Bit fields for configuring IOAPIC Redirection Table Entries.
 */
/**@{*/
#define APIC_IOAPIC_DELIVERY_MODE_FIXED            (0 << 8) /**< @brief Delivery mode: Fixed. */
#define APIC_IOAPIC_DELIVERY_MODE_LOWEST_PRIORITY  (1 << 8) /**< @brief Delivery mode: Lowest Priority. */
#define APIC_IOAPIC_DELIVERY_MODE_SMI              (2 << 8) /**< @brief Delivery mode: SMI. */
#define APIC_IOAPIC_DELIVERY_MODE_NMI              (4 << 8) /**< @brief Delivery mode: NMI. */
#define APIC_IOAPIC_DELIVERY_MODE_INIT             (5 << 8) /**< @brief Delivery mode: INIT. */
#define APIC_IOAPIC_DELIVERY_MODE_EXTERNAL_INT     (7 << 8) /**< @brief Delivery mode: External Interrupt. */
#define APIC_IOAPIC_DESTINATION_MODE_PHYSICAL      (0 << 11) /**< @brief Destination mode: Physical. */
#define APIC_IOAPIC_DESTINATION_MODE_LOGICAL       (1 << 11) /**< @brief Destination mode: Logical. */
#define APIC_IOAPIC_DELIVERY_STATUS_RELAX          (0 << 12) /**< @brief Delivery status: Relaxed. */
#define APIC_IOAPIC_DELIVERY_STATUS_WAITING        (1 << 12) /**< @brief Delivery status: Waiting. */
#define APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH       (0 << 13) /**< @brief Pin polarity: Active High. */
#define APIC_IOAPIC_PIN_POLARITY_ACTIVE_LOW        (1 << 13) /**< @brief Pin polarity: Active Low. */
#define APIC_IOAPIC_TRIGGER_MODE_EDGE              (0 << 15) /**< @brief Trigger mode: Edge. */
#define APIC_IOAPIC_TRIGGER_MODE_LEVEL             (1 << 15) /**< @brief Trigger mode: Level. */
#define APIC_IOAPIC_INTERRUPT_ENABLED              (0 << 16) /**< @brief Interrupt mask: Enabled. */
#define APIC_IOAPIC_INTERRUPT_DISABLED             (1 << 16) /**< @brief Interrupt mask: Disabled. */
/**@}*/

/**
 * @brief Macro to extract the maximum number of redirection entries from the IOAPIC version register.
 *
 * @param r The value of the IOAPIC version register.
 * @return The maximum number of redirection entries supported by the IOAPIC.
 */
#define APIC_IOAPIC_MAX_REDIRECTION_ENTRY(r)  (((r >> 16) & 0xFF) + 1)

/**
 * @brief Alias for APIC_IOAPIC_INTERRUPT_ENABLED.
 */
#define APIC_INTERRUPT_ENABLED  APIC_IOAPIC_INTERRUPT_ENABLED
/** @brief Alias for APIC_IOAPIC_INTERRUPT_DISABLED. */
#define APIC_INTERRUPT_DISABLED APIC_IOAPIC_INTERRUPT_DISABLED

/**
 * @name APIC Timer Modes
 * @brief Modes for the APIC Timer.
 */
/**@{*/
#define APIC_TIMER_ONESHOT          (0 << 17) /**< @brief Timer mode: One-shot. */
#define APIC_TIMER_PERIODIC         (1 << 17) /**< @brief Timer mode: Periodic. */
#define APIC_TIMER_TSC_DEADLINE     (2 << 17) /**< @brief Timer mode: TSC Deadline. */
/**@}*/

/**
 * @struct apic_ioapic_register_t
 * @brief Represents an I/O APIC register pair (selector and value).
 *
 * This structure is used to access I/O APIC registers, which typically involve
 * writing to a selector register first, then to the value register.
 */
typedef struct apic_ioapic_register_t {
    uint32_t selector; /**< @brief Register address selector. */
    uint32_t reserved0[3]; /**< @brief Reserved space. */
    uint32_t value; /**< @brief Register data value. */
}__attribute__((packed)) apic_ioapic_register_t;

/**
 * @struct apic_register_spurious_interrupt_t
 * @brief Structure for the APIC Spurious Interrupt Vector Register (SIVR).
 *
 * This structure defines the layout of the SIVR, including the interrupt vector,
 * APIC software enable bit, and focus CPU core checking bit.
 */
typedef struct apic_register_spurious_interrupt_t {
    uint8_t  vector                  : 8; /**< @brief Interrupt vector number. */
    uint8_t  apic_software_enable    : 1; /**< @brief APIC software enable bit. */
    uint8_t  focus_cpu_core_checking : 1; /**< @brief Focus CPU core checking bit. */
    uint32_t reserved0               : 22; /**< @brief Reserved bits. */
}__attribute__((packed)) apic_register_spurious_interrupt_t;

/**
 * @struct apic_lintv_t
 * @brief Structure for APIC Local Vector Table (LVT) entries (e.g., LINT0, LINT1).
 *
 * This structure defines the fields within an LVT entry, such as the interrupt vector,
 * delivery mode, mask bit, and delivery status.
 */
typedef struct apic_lintv_t {
    uint8_t  vector          : 8; /**< @brief Interrupt vector. */
    uint8_t  delivery_mode   : 3; /**< @brief Delivery mode. */
    uint8_t  reserved0       : 1; /**< @brief Reserved bit. */
    uint8_t  delivery_status : 1; /**< @brief Delivery status. */
    uint8_t  reserved1       : 3; /**< @brief Reserved bits. */
    uint8_t  mask            : 1; /**< @brief Mask bit (1 = masked, 0 = unmasked). */
    uint16_t reserved2       : 15; /**< @brief Reserved bits. */
}__attribute__((packed)) apic_lintv_t;

/**
 * @brief Initializes the APIC subsystem.
 *
 * This function parses the ACPI MADT (Multiple APIC Description Table) to
 * discover and configure Local APICs and I/O APICs.
 *
 * @param desc Pointer to the RSDP (Root System Description Table) descriptor.
 * @return 0 on success, or a negative error code on failure.
 */
int8_t apic_setup(acpi_xrsdp_descriptor_t* desc);

/**
 * @brief Restores the I/O APIC state after waking up from a low-power state.
 *
 * This function is typically called during system resume to reconfigure the I/O APIC
 * based on the information gathered during APIC setup.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int8_t apic_restore_ioapic_after_wakeup(void);

/**
 * @brief Initializes the APIC timer.
 *
 * Configures the Local APIC timer for use, typically for system timing or scheduling.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int8_t apic_init_timer(void);

/**
 * @brief Configures an IRQ line on the I/O APIC.
 *
 * Sets up the properties for a specific IRQ line, such as enabling/disabling it,
 * setting its polarity, trigger mode, and delivery mode.
 *
 * @param irq The IRQ number to configure.
 * @param props The properties to set for the IRQ line (using APIC_IOAPIC_* macros).
 * @return 0 on success, or a negative error code on failure.
 */
int8_t apic_ioapic_setup_irq(uint8_t irq, uint32_t props);

/**
 * @brief Enables or disables an IRQ line on the I/O APIC.
 *
 * This function modifies the interrupt mask bit for a given IRQ line.
 *
 * @param irq The IRQ number to switch.
 * @param disabled If 1, the IRQ is disabled; if 0, it is enabled.
 * @return 0 on success, or a negative error code on failure.
 */
int8_t apic_ioapic_switch_irq(uint8_t irq, uint32_t disabled);

/**
 * @brief Enables a specific IRQ line on the I/O APIC.
 *
 * @param irq The IRQ number to enable.
 */
#define apic_ioapic_enable_irq(irq) apic_ioapic_switch_irq(irq, 0)

/**
 * @brief Disables a specific IRQ line on the I/O APIC.
 *
 * @param irq The IRQ number to disable.
 */
#define apic_ioapic_disable_irq(irq) apic_ioapic_switch_irq(irq, 1)

/**
 * @brief Gets the overridden IRQ number for a given old IRQ.
 *
 * This is useful for handling IRQ remapping, often defined in the ACPI ISA
 * interrupt override entries.
 *
 * @param old_irq The original IRQ number.
 * @return The potentially remapped IRQ number.
 */
uint8_t apic_get_irq_override(uint8_t old_irq);

/**
 * @brief Sends an End-Of-Interrupt (EOI) signal to the APIC.
 *
 * This function must be called by an interrupt handler after it has finished
 * processing an interrupt to acknowledge it to the APIC.
 */
void apic_eoi(void);

/**
 * @brief Sends an Inter-Processor Interrupt (IPI) to a destination APIC.
 *
 * @param destination The APIC ID of the destination processor.
 * @param vector The interrupt vector to send.
 * @param wait If true, the function will wait for the IPI to be sent.
 */
void apic_send_ipi(uint8_t destination, uint8_t vector, boolean_t wait);

/**
 * @brief Sends an INIT IPI to a destination processor.
 *
 * Used to reset or initialize a target processor.
 *
 * @param destination The APIC ID of the destination processor.
 */
void apic_send_init(uint8_t destination);

/**
 * @brief Sends a Startup IPI (SIPI) to a destination processor.
 *
 * Used to start execution on a target processor at a specified address.
 *
 * @param destination The APIC ID of the destination processor.
 * @param vector The interrupt vector to start execution from.
 */
void apic_send_sipi(uint8_t destination, uint8_t vector);

/**
 * @brief Sends a Non-Maskable Interrupt (NMI) IPI to a destination processor.
 *
 * @param destination The APIC ID of the destination processor.
 */
void apic_send_nmi(uint8_t destination);

/**
 * @brief Enables the Local APIC on the current processor.
 *
 * This function typically involves setting the APIC Base Address MSR and
 * enabling the APIC globally.
 */
void apic_enable_lapic(void);

/**
 * @brief Configures the Local APIC for the current processor.
 *
 * This function performs initial setup of the Local APIC, potentially including
 * setting the Spurious Interrupt Vector Register and LVT entries.
 *
 * @return The configured APIC ID of the local processor, or a negative value on error.
 */
uint8_t apic_configure_lapic(void);

/**
 * @brief Gets the total count of active application processors (APs).
 *
 * This function relies on information gathered during APIC setup, typically from
 * the MADT.
 *
 * @return The number of application processors detected.
 */
uint64_t apic_get_ap_count(void);

/**
 * @brief Checks if the APIC timer is currently active or waiting.
 *
 * @return TRUE if the timer is active/waiting, FALSE otherwise.
 */
boolean_t apic_is_waiting_timer(void);

/**
 * @brief Retrieves the vector of the first pending interrupt in the Interrupt Request Register (IRR).
 *
 * This function checks the IRR registers to find the highest priority pending interrupt.
 *
 * @return The interrupt vector number, or a negative value if no interrupts are pending.
 */
int32_t apic_get_first_irr_interrupt(void);

/**
 * @brief Retrieves the vector of the highest priority pending interrupt in the Interrupt Service Register (ISR).
 *
 * This function checks the ISR registers to find the highest priority interrupt currently being serviced.
 *
 * @return The interrupt vector number, or a negative value if no interrupts are being serviced.
 */
int32_t apic_get_isr_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif
