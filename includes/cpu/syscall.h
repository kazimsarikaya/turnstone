/**
 * @file syscall.h
 * @brief syscall interface for turnstone 64bit kernel.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 **/

#ifndef ___CPU_SYSCALL_H
/*! prevent duplicate header error macro */
#define ___CPU_SYSCALL_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct syscall_frame_t {
    uint64_t rsp; ///< rsp register value
    uint64_t cr3; ///< cr3 register value
    uint64_t rax; ///< rax register value
    uint64_t rbx; ///< rbx register value
    uint64_t rcx; ///< rcx register value
    uint64_t rdx; ///< rdx register value
    uint64_t rsi; ///< rsi register value
    uint64_t rdi; ///< rdi register value
    uint64_t rbp; ///< rbp register value
    uint64_t r8; ///< r8 register value
    uint64_t r9; ///< r9 register value
    uint64_t r10; ///< r10 register value
    uint64_t r11; ///< r11 register value
    uint64_t r12; ///< r12 register value
    uint64_t r13; ///< r13 register value
    uint64_t r14; ///< r14 register value
    uint64_t r15; ///< r15 register value
    uint8_t  avx512f[0x2000 + 0x80]; ///< avx512f registers 0x40 bytes for dynamic alignment
} __attribute__((packed)) syscall_frame_t; ///< struct short hand

_Static_assert(sizeof(syscall_frame_t) == 0x2108, "interrupt_frame_ext_t size must be 0x2108");

typedef int64_t (*syscall_generic_handler_f)(syscall_frame_t* frame);

void syscall_handlers_set_generic_handler(syscall_generic_handler_f handler);

void syscall_handler(void);

void syscall_init_generic_handler(void);

void syscall_init(void);

#ifdef __cplusplus
}
#endif

#endif /* !___CPU_SYSCALL_H */
