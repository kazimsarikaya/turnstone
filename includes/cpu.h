/**
 * @file cpu.h
 * @brief cpu commands which needs assembly codes.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
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
__attribute__((noreturn)) void cpu_hlt();

void cpu_idle();

/**
 * @brief disables interrupts
 *
 * This command disables interrupts with cli assembly command.
 */
static inline void cpu_cli() {
    __asm__ __volatile__ ("cli");
}

/**
 * @brief enables interrupts
 *
 * This command enables interrupts with sti assembly command.
 */
static inline void cpu_sti() {
    __asm__ __volatile__ ("sti");
}

/**
 * @brief nop instruction
 *
 * This command nops.
 */
static inline void cpu_nop() {
    __asm__ __volatile__ ("nop");
}

/**
 * @brief cld instruction
 *
 * This command clears direction flag.
 */
static inline void cpu_cld() {
    __asm__ __volatile__ ("cld");
}

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

/**
 * @brief clears segments
 *
 * sets segment registers to zero
 */
void cpu_clear_segments();

/**
 * @brief prepares stack
 * @param[in] stack_address stack top value
 *
 * sets esp and clears ebp
 */
static inline void cpu_set_and_clear_stack(uint64_t stack_address) {
    __asm__ __volatile__ (
        "mov %%rax, %%rsp\n"
        "xor %%rbp, %%rbp\n"
        : : "a" (stack_address)
        );
}

/**
 * @brief invalidates tlb for address
 * @param[in] address adddress to invalidate
 *
 * calls invlpg instruction
 */
static inline void cpu_tlb_invalidate(void * address) {
    __asm__ __volatile__ ("invlpg (%%rax)" : : "a" (address));
}

static inline void cpu_tlb_flush() {
    __asm__ __volatile__ ("mov %%cr3, %%rax\n"
                          "mov %%rax, %%cr3"
                          ::: "rax");
}

#endif
