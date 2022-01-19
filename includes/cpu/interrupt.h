/**
 * @file interrupt.h
 * @brief defines interrupt functions at long mode
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
	uint64_t empty1 : 48; ///< unused value
	uint64_t return_rflags; ///< the rflags after interrupt
	uint64_t return_rsp; ///< the rsp value aka stack after interrupt
	uint64_t return_ss : 16; ///< the ss value aka stack segment after interrupt
	uint64_t empty2 : 48; ///< unused value
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

#endif
