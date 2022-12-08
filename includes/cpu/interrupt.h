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

/**
 * @brief interrupt error code type
 */
#define interrupt_errcode_t uint64_t

#define INTERRUPT_IRQ_BASE        0x20
#define INTERRUPT_VECTOR_SPURIOUS 0xFF

/**
 * @struct interrupt_frame
 * @brief  interrupt frame for interrupt function
 */
typedef struct interrupt_frame {
    uint64_t return_rip; ///< the ip value after interrupt
    uint64_t return_cs : 16; ///< the cs value after intterupt
    uint64_t empty1    : 48; ///< unused value
    uint64_t return_rflags; ///< the rflags after interrupt
    uint64_t return_rsp; ///< the rsp value aka stack after interrupt
    uint64_t return_ss : 16; ///< the ss value aka stack segment after interrupt
    uint64_t empty2    : 48; ///< unused value
} __attribute__((packed)) interrupt_frame_t; ///< struct short hand

/**
 * @brief interrupt table builder functions
 *
 * builds interrupt table at idtr look for @ref descriptor_idt
 */
int8_t interrupt_init();

typedef int8_t (* interrupt_irq)(interrupt_frame_t* frame, uint8_t intnum);

int8_t interrupt_irq_set_handler(uint8_t irqnum, interrupt_irq irq);
int8_t interrupt_irq_remove_handler(uint8_t irqnum, interrupt_irq irq);

uint8_t interrupt_get_next_empty_interrupt();

typedef union {
    struct {
        uint32_t present           : 1;
        uint32_t write             : 1;
        uint32_t user              : 1;
        uint32_t reserved_bits     : 1;
        uint32_t instruction_fetch : 1;
        uint32_t protected_keys    : 1;
        uint32_t shadow_stack      : 1;
        uint32_t reserved1         : 8;
        uint32_t sgx               : 1;
        uint32_t reserved2         : 16;
    } __attribute__((packed)) fields;
    uint32_t bits;
} interrupt_errorcode_pagefault_t;

#endif
