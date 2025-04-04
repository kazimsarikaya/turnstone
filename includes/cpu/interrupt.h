/**
 * @file interrupt.h
 * @brief defines interrupt functions at long mode
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___CPU_INTERRUPT_H
/*! prevent duplicate header error macro */
#define ___CPU_INTERRUPT_H 0

#include <types.h>
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief interrupt error code type
 */
#define interrupt_errcode_t uint64_t

/*! the base interrupt number of irqs */
#define INTERRUPT_IRQ_BASE        0x20
/*! apci spurious interrupt number */
#define INTERRUPT_VECTOR_SPURIOUS 0xFF

/**
 * @struct interrupt_frame
 * @brief  interrupt frame for interrupt function
 */
typedef struct interrupt_frame_t {
    uint64_t return_rip; ///< the ip value after interrupt
    uint64_t return_cs : 16; ///< the cs value after intterupt
    uint64_t empty1    : 48; ///< unused value
    uint64_t return_rflags; ///< the rflags after interrupt
    uint64_t return_rsp; ///< the rsp value aka stack after interrupt
    uint64_t return_ss : 16; ///< the ss value aka stack segment after interrupt
    uint64_t empty2    : 48; ///< unused value
} __attribute__((packed)) interrupt_frame_t; ///< struct short hand

/**
 * @struct interrupt_frame_ext
 * @brief  interrupt frame for interrupt function
 */
typedef struct interrupt_frame_ext_t {
    uint64_t rsp; ///< rsp register value
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
    uint64_t interrupt_number; ///< interrupt number
    uint64_t error_code; ///< error code
    uint64_t return_rip; ///< the ip value after interrupt
    uint64_t return_cs : 16; ///< the cs value after intterupt
    uint64_t empty1    : 48; ///< unused value
    uint64_t return_rflags; ///< the rflags after interrupt
    uint64_t return_rsp; ///< the rsp value aka stack after interrupt
    uint64_t return_ss : 16; ///< the ss value aka stack segment after interrupt
    uint64_t empty2    : 48; ///< unused value
} __attribute__((packed)) interrupt_frame_ext_t; ///< struct short hand

_Static_assert(sizeof(interrupt_frame_ext_t) == 0x2138, "interrupt_frame_ext_t size must be 0x2138");

/**
 * @brief interrupt table builder functions
 *
 * builds interrupt table at idtr look for @ref descriptor_idt
 */
int8_t interrupt_init(void);


/**
 * @brief interrupt redirector for main interrupts
 * @param[in] ist interrupt stack table index
 * @return 0 if succeed
 */
int8_t interrupt_ist_redirect_main_interrupts(uint8_t ist);

/**
 * @brief interrupt redirector for main interrupts
 * @param[in] ist interrupt stack table index
 * @return 0 if succeed
 */
int8_t interrupt_ist_redirect_interrupt(uint8_t vec, uint8_t ist);

/**
 * @brief irq method signature
 * @param[in] frame interrupt frame
 * @param[in] intnum interrupt/irq number
 * @return 0 if irq handled success fully.
 */
typedef int8_t (* interrupt_irq)(interrupt_frame_ext_t* frame);

/**
 * @brief registers irq for irq number, for an irq there can be more irq handlers, loops with return of @ref interrupt_irq
 * @param[in] irqnum irq number to handle
 * @param[in] irq the irq handler
 * @return 0 if succeed
 */
int8_t interrupt_irq_set_handler(uint8_t irqnum, interrupt_irq irq);

/**
 * @brief remove irq for irq number
 * @param[in] irqnum irq number to handle
 * @param[in] irq the irq handler
 * @return 0 if succeed
 */
int8_t interrupt_irq_remove_handler(uint8_t irqnum, interrupt_irq irq);

/**
 * @brief finds not used interrupt for registering
 * @return interrupt number
 */
uint8_t interrupt_get_next_empty_interrupt(void);

/**
 * @union interrupt_errorcode_pagefault_u
 * @brief bit fileds for page fault error no
 */
typedef union interrupt_errorcode_pagefault_u {
    struct {
        uint32_t present           : 1; ///< fault ocurred when page exits, if 0, then the page mapping not found
        uint32_t write             : 1; ///< fault occured to writing to a readonly page
        uint32_t user              : 1; ///< fault occured user mode accesses
        uint32_t reserved_bits     : 1; ///< fault occured when reserved bits edited at page
        uint32_t instruction_fetch : 1; ///< fault occured when instruction fetch
        uint32_t protected_keys    : 1; ///< fault is for protected keys
        uint32_t shadow_stack      : 1; ///< fault is for shadow stack
        uint32_t reserved1         : 8; ///< reserved
        uint32_t sgx               : 1; ///< fault is for sgx
        uint32_t reserved2         : 16; ///< reserved
    } __attribute__((packed)) fields; ///< bit fields
    uint32_t bits; ///< all value in 32bit integer
} interrupt_errorcode_pagefault_t; ///< union short hand for @ref interrupt_errorcode_pagefault_u

void interrupt_generic_handler(interrupt_frame_ext_t* frame);

#ifdef __cplusplus
}
#endif

#endif
