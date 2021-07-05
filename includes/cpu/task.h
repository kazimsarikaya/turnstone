/**
 * @file task.h
 * @brief cpu task definitons
 */
#ifndef ___TASK_H
/*! prevent duplicate header error macro */
#define ___TASK_H 0

#include <cpu/descriptor.h>
#include <cpu/interrupt.h>
#include <memory.h>
#include <memory/paging.h>

#define TASK_TSS_STACK_START 0x200000
#define TASK_TSS_STACK_STEP  (176 << 10)

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


typedef struct task_register_store {
	uint64_t local2;
	uint64_t local1;
	uint64_t return_rsp1; ///< rsp inside generic handler
	uint64_t return_rbp1; ///< rbp inside generic handler
	uint64_t return_rip1; ///< return rip of real interrupt inside generic handler
	uint64_t rax;
	uint64_t rbx;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t return_rflags1; ///< rflags inside generic handler
	uint64_t return_rip0; ///< the ip value after interrupt
	uint16_t return_cs0; ///< the cs value after intterupt
	uint64_t empty1 : 48; ///< unused value
	uint64_t return_rflags0; ///< the rflags after interrupt
	uint64_t return_rsp0; ///< the rsp value aka stack after interrupt
	uint16_t return_ss0; ///< the ss value aka stach segment after interrupt
	uint64_t empty2 : 48; ///< unused value
	uint8_t* fx_registers;
}__attribute__((packed)) task_register_store_t;


typedef struct {
	uint64_t pid;
	task_register_store_t* register_store;
	void* entry_point;
	memory_page_table_t* page_table;
	uint64_t rsp;
} task_t;

int8_t task_init_tasking_ext(memory_heap_t* heap);
#define task_init_tasking() task_init_tasking_ext(NULL)

void task_switch_task();

#endif
