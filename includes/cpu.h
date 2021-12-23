/**
 * @file cpu.h
 * @brief cpu commands which needs assembly codes.
 *
 */
#ifndef ___CPU_H
/*! prevent duplicate header error macro */
#define ___CPU_H 0

#include <types.h>

/**
 * @brief stops cpu.
 *
 * This command stops cpu using hlt assembly command inside for.
 */
void cpu_hlt();

/**
 * @brief disables interrupts
 *
 * This command disables interrupts with cli assembly command.
 */
void cpu_cli();

/**
 * @brief enables interrupts
 *
 * This command enables interrupts with sti assembly command.
 */
void cpu_sti();

/**
 * @brief reads data segment (ds) value from cpu.
 * @return ds value
 *
 * This function is meaningful at real mode.
 * At long mode is does nothing
 */
uint16_t cpu_read_data_segment();

/**
 * @brief checks rdrand supported
 * @return 0 when supported else -1
 */
int8_t cpu_check_rdrand();

/**
 * @brief read msr and return
 * @param[in]  msr_address model Specific register address
 * @return             msr value
 *
 * returns edx:eax as uint64_t
 */
uint64_t cpu_read_msr(uint32_t msr_address);

/**
 * @brief writes msr
 * @param[in]  msr_address model Specific register address
 * @param[in]  value       value to write
 * @return             0 on sucess
 *
 * parse value into edx:eax
 */
int8_t cpu_write_msr(uint32_t msr_address, uint64_t value);

/**
 * @brief read cr2 and return
 * @return             cr2 value
 *
 * returns cr2 value for page faults
 */
uint64_t cpu_read_cr2();

/**
 * @struct cpu_cpuid_regs
 * @brief  registers
 * @brief cpuid command set/get registers.
 *
 * cpuid command needs setting eax and results are returned for registers.
 */
typedef struct cpu_cpuid_regs {
	uint32_t eax; ///< eax register
	uint32_t ebx; ///< ebx register
	uint32_t ecx; ///< ecx register
	uint32_t edx; ///< edx register
} cpu_cpuid_regs_t; ///< struct short hand

uint8_t cpu_cpuid(cpu_cpuid_regs_t query, cpu_cpuid_regs_t* answer);

#endif
