/**
 * @file crx.h
 * @brief defines crx registers detailed
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___CPU_CRX_H
/*! prevent duplicate header error macro */
#define ___CPU_CRX_H 0

#include <types.h>

/**
 * @struct cpu_reg_cr0_s
 * @brief cr0 register bit fields
 */
typedef struct cpu_reg_cr0_s {
    uint8_t  protection_enabled  : 1; ///< when set protected mode activated
    uint8_t  monitor_coprocessor : 1; ///< when set co-processor works
    uint8_t  emulation           : 1; ///< when x87 fpu emuation enabled
    uint8_t  task_switched       : 1; ///< hardware task switching
    uint8_t  extension_type      : 1; ///< extension type
    uint8_t  numeric_error       : 1; ///< numbmeric errors at fpu
    uint16_t reserved1           : 10; ///< reserved
    uint8_t  write_protect       : 1; ///< when set kernel cannot modify readonly pages
    uint8_t  reserved2           : 1; ///< reserved
    uint8_t  alignment_mask      : 1; ///<  alligmnet mask
    uint16_t reserved3           : 10; ///< reserved
    uint8_t  not_writethrough    : 1; ///< when set cache is not wrrite through
    uint8_t  cache_disable       : 1; ///< when set cpu cache disabled
    uint8_t  paging              : 1; ///< when set paging starts in long mode always one
    uint32_t reserved4           : 32; ///< reserved
}__attribute__((packed)) cpu_reg_cr0_t;  ///< shard hand for struct @ref cpu_reg_cr0_s

/**
 * @union cpu_reg_cr4_u
 * @brief cr4 register bit fields
 */
typedef union cpu_reg_cr4_u {
    struct {
        uint8_t  virtual_mode_extensions               : 1; ///< when set enables vme
        uint8_t  protected_mode_virtual_ints           : 1; ///< when set eables virtual interrupts at protected mode
        uint8_t  timestamp_disable                     : 1; ///< disables timestamp
        uint8_t  debugging_extensions                  : 1; ///< enables debugging
        uint8_t  page_size_extensions                  : 1; ///< changes page size
        uint8_t  physical_address_extension            : 1; ///< enables memory size over 4gib when 32bit
        uint8_t  machine_check_enable                  : 1; ///< enables machine check
        uint8_t  page_global_enable                    : 1; ///< enables global pages at page time
        uint8_t  performance_monitoring_counter_enable : 1; ///< enables performance monitoring
        uint8_t  os_fx_support                         : 1; ///< enables fx (sse)
        uint8_t  os_unmasked_exception_support         : 1; ///< enables unmasked simd interrupts
        uint8_t  user_mode_instruction_preventation    : 1; ///< disables several instructions at user mode for descriptors
        uint8_t  reserved1                             : 4; ///< reserved
        uint8_t  fs_gs_base_enable                     : 1; ///< enables changing base address of fs and gs registers
        uint8_t  process_context_identifier_enable     : 1; ///< enables process context identifier at cr3
        uint8_t  os_xsave_eable                        : 1; ///< enables xsave instruction
        uint8_t  reserved2                             : 1; ///< reserved
        uint8_t  supervisor_mode_execution_prevention  : 1; ///< enables kernel exection of user space code with rflags
        uint8_t  supervisor_mode_access_protection     : 1; ///< enables kernel memory access of user space with rflags
        uint8_t  protection_key_enabled                : 1; ///< enables prorection key
        uint8_t  control_flow_enforcement_technology   : 1; ///< enables control flow
        uint8_t  reserved3                             : 8; ///< reserved
        uint32_t reserved4                             : 32; ///< reserved
    }__attribute__((packed)) fields;  ///< bit field list
    uint64_t bits;  ///< all bits in one 64 bit value
} cpu_reg_cr4_t;  ///< short hand for union @ref cpu_reg_cr4_u

/**
 * @brief reads cr4 register
 * @return cr4 value
 */
cpu_reg_cr4_t cpu_read_cr4();

/**
 * @brief writes cr4 register
 * @param[in] cr4 cr4 value
 */
void cpu_write_cr4(cpu_reg_cr4_t cr4);

/**
 * @brief reads cr0 register
 * @return cr0 value
 */
cpu_reg_cr0_t cpu_read_cr0();

/**
 * @brief writes cr0 register
 * @param[in] cr0 cr0 value
 */
void cpu_write_cr0(cpu_reg_cr0_t cr0);

/**
 * @brief toggles write protect bit cr0, when setted kernel can not modify readonly pages
 */
void cpu_toggle_cr0_wp();

/**
 * @brief enables sse support, modifies cr4 register
 */
void cpu_enable_sse();

#endif
