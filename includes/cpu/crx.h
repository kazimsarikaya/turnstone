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

typedef struct {
	uint8_t protection_enabled : 1;
	uint8_t monitor_coprocessor : 1;
	uint8_t emulation : 1;
	uint8_t task_switched : 1;
	uint8_t extension_type : 1;
	uint8_t numeric_error : 1;
	uint16_t reserved1 : 10;
	uint8_t write_protect : 1;
	uint8_t reserved2 : 1;
	uint8_t alignment_mask : 1;
	uint16_t reserved3 : 10;
	uint8_t not_writethrough : 1;
	uint8_t cache_disable : 1;
	uint8_t paging : 1;
	uint32_t reserved4 : 32;
}__attribute__((packed)) cpu_reg_cr0_t;


typedef union {
	struct {
		uint8_t virtual_mode_extensions : 1;
		uint8_t protected_mode_virtual_ints : 1;
		uint8_t timestamp_disable : 1;
		uint8_t debugging_extensions : 1;
		uint8_t page_size_extensions : 1;
		uint8_t physical_address_extension : 1;
		uint8_t machine_check_enable : 1;
		uint8_t page_global_enable : 1;
		uint8_t performance_monitoring_counter_enable : 1;
		uint8_t os_fx_support : 1;
		uint8_t os_unmasked_exception_support : 1;
		uint8_t user_mode_instruction_preventation : 1;
		uint8_t reserved1 : 4;
		uint8_t fsbase_enable : 1;
		uint8_t process_context_identifier_enable : 1;
		uint8_t os_xsave_eable : 1;
		uint8_t reserved2 : 1;
		uint8_t supervisor_mode_execution_prevention : 1;
		uint8_t supervisor_mode_access_protection : 1;
		uint8_t protection_key_enabled : 1;
		uint8_t control_flow_enforcement_technology : 1;
		uint8_t reserved3 : 8;
		uint32_t reserved4 : 32;
    }__attribute__((packed)) fields;
	uint64_t bits;
} cpu_reg_cr4_t;

cpu_reg_cr4_t cpu_read_cr4();
void cpu_write_cr4(cpu_reg_cr4_t cr4);

cpu_reg_cr0_t cpu_read_cr0();
void cpu_write_cr0(cpu_reg_cr0_t cr0);
void cpu_toggle_cr0_wp();

#endif
