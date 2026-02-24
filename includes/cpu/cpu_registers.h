/**
 * @file cpu_registers.h
 * @brief CPU Register Definitions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___CPU_REGISTERS_H
#define ___CPU_REGISTERS_H 0

#include <types.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct cpu_registers_t {
    uint64_t rax; ///< register
    uint64_t rbx; ///< register
    uint64_t rcx; ///< register
    uint64_t rdx; ///< register
    uint64_t r8; ///< register
    uint64_t r9; ///< register
    uint64_t r10; ///< register
    uint64_t r11; ///< register
    uint64_t r12; ///< register
    uint64_t r13; ///< register
    uint64_t r14; ///< register
    uint64_t r15; ///< register
    uint64_t rsi; ///< register
    uint64_t rdi; ///< register
    uint64_t rsp; ///< register
    uint64_t rbp; ///< register
    uint64_t rflags; ///< register
    uint64_t cr3; ///< register
    uint32_t xsave_mask_lo; ///< xsave mask low
    uint32_t xsave_mask_hi; ///< xsave mask high
    uint8_t  avx512f[0x2000] __attribute__((aligned(0x40))); ///< register
} cpu_registers_t;

_Static_assert(sizeof(cpu_registers_t) == 0x20c0, "cpu_registers_t size must be 0x20c0");
_Static_assert((offsetof_field(cpu_registers_t, avx512f) % 0x40) == 0x0, "cpu_registers_t avx512f offset must be aligned 0x40");

#ifdef __cplusplus
}
#endif

#endif /* ___CPU_REGISTERS_H */
