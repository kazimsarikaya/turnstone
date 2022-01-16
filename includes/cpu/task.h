/**
 * @file task.h
 * @brief cpu task definitons
 */
#ifndef ___CPU_TASK_H
/*! prevent duplicate header error macro */
#define ___CPU_TASK_H 0

#include <cpu/descriptor.h>
#include <cpu/interrupt.h>
#include <memory.h>
#include <memory/paging.h>
#include <linkedlist.h>

#define TASK_MAX_TICK_COUNT 50

#define TASK_KERNEL_TASK_ID 1

/**
 * @struct descriptor_tss
 * @brief 64 bit tss descriptor
 */
typedef struct descriptor_tss {
	uint16_t segment_limit1 : 16; ///< segment limit bits 0-15
	uint32_t base_address1 : 24; ///< base address bits 0-23
	uint8_t type : 4; ///< tss type 0x9 in bits
	uint8_t always0_1 : 1; ////< always zero
	uint8_t dpl : 2; ///<  privilage level
	uint8_t present : 1; ///< 2/15 aka 47 is always 1
	uint8_t segment_limit2 : 4; ///< segment limit bits 16-19
	uint8_t unused1 : 1; ///< unused bit os can use it
	uint8_t always0_2 : 2; ////< always zero
	uint8_t long_mode : 1; ///< this bit is always 1 for long mode aka granularity
	uint64_t base_address2 : 40; ///< base address bits 24-63
	uint8_t reserved0 : 8; ///< reserved bits
	uint16_t always0_3 : 5; ///< always zero
	uint32_t reserved1 : 19; ///< reserved bits
}__attribute__((packed)) descriptor_tss_t;

/**
 * @brief initialize tss
 * @param  tss   address of tss slot in gdt
 * @param  base  base address of tss struct
 * @param  limit length of tss - 1
 * @param  DPL   privilage level
 */
#define DESCRIPTOR_BUILD_TSS_SEG(tss, base, limit, DPL) { \
		tss->type = SYSTEM_SEGMENT_TYPE_TSS_A; \
		tss->present = 1; \
		tss->long_mode = 1; \
		tss->dpl = DPL; \
		tss->segment_limit1 = limit & 0xFFFF; \
		tss->segment_limit2 = (limit >> 16) & 0xF; \
		tss->base_address1 = base & 0xFFFFFF; \
		tss->base_address2 = (base >> 24); \
}

typedef struct {
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base_address;
}__attribute__((packed)) tss_t;

typedef enum {
	TASK_STATE_CREATED,
	TASK_STATE_ENDED
} task_state_t;

typedef struct {
	memory_heap_t* heap;
	uint64_t task_id;
	uint64_t last_tick_count;
	task_state_t state;
	void* entry_point;
	void* stack;
	uint64_t stack_size;
	linkedlist_t message_queues;
	boolean_t message_waiting;
	memory_page_table_t* page_table;
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rsp;
	uint64_t rbp;
	uint8_t* fx_registers;
	uint64_t rflags;
} task_t;

int8_t task_init_tasking_ext(memory_heap_t* heap);
#define task_init_tasking() task_init_tasking_ext(NULL)

void task_switch_task();
void task_yield();
uint64_t task_get_id();
task_t* task_get_current_task();

void task_set_message_waiting();
void task_add_message_queue(linkedlist_t queue);

int8_t task_create_task(memory_heap_t* heap, uint64_t stack_size, void* entry_point);

#endif
