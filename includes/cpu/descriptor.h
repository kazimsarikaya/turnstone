/**
 * @file descriptor.h
 * @brief cpu segment descriptors for long mode.
 */
#ifndef ___CPU_DESCRIPTOR_H
/*! prevent duplicate header error macro */
#define ___CPU_DESCRIPTOR_H 0

#include <types.h>

/*! cpu privilage level 0 for kernel space */
#define DPL_RING0 0
/*! cpu privilage level 1 for kernel additional space */
#define DPL_RING1 1
/*! cpu privilage level 2 for kernel additional space */
#define DPL_RING2 2
/*! cpu privilage level 3 for user space */
#define DPL_RING3 3

/*! a short hand for ring0 aka kernel space */
#define DPL_KERNEL DPL_RING0
/*! a short hand for ring3 aka user space */
#define DPL_USER   DPL_RING3

/*! hard coded gdt code segment value */
#define KERNEL_CODE_SEG 0x08

/**
 * @struct descriptor_gdt_null
 * @brief  null segment descriptor for gdt
 */
typedef struct descriptor_gdt_null {
	uint32_t unused1; ///< null segment is always 0
	uint32_t unused2; ///< null segment is always 0
}__attribute__((packed)) descriptor_gdt_null_t; ///< struct short hand

/**
 * @struct descriptor_gdt_code
 * @brief  code segment descriptor for gdt
 */
typedef struct descriptor_gdt_code {
	uint32_t unused1; ///< 1/0-31 are unused
	uint16_t unused2 : 10;  ///< 2/0-9 aka 32-41 are unused
	uint8_t conforming : 1; ///< 2/10 aka 42 conforming allow diffrent levels run code
	uint8_t always1 : 2; ///< 2/11-12 aka 43-44 are always 1
	uint8_t dpl : 2; ///< 2/13-14 aka 45-46 bits are for privilage levels
	uint8_t present : 1; ///< 2/15 aka 47 the page or page table is present?
	uint8_t unused3 : 4; ///< 2/16-19 aka 48-51 are unused
	uint8_t avl : 1; ///< 2/20 aka 52 are avl bits aka unused
	uint8_t long_mode : 1; ///< 2/21 aka 53 this bit is always 1 for long mode
	uint8_t default_opsize : 1; ///< 2/22 aka 54 this bit is always 0 for long mode
	uint8_t unused4 : 1; ///< 2/23 aka 55 is unused
	uint8_t unused5; ///< 2/24-31 aka 56-63 is unused

}__attribute__((packed)) descriptor_gdt_code_t; ///< struct short hand

/**
 * @struct descriptor_gdt_data
 * @brief data segment descriptor for gdt
 */
typedef struct descriptor_gdt_data {
	uint32_t unused1; ///< 1/0-31 are unused
	uint16_t unused2 : 11;  ///< 2/0-10 aka 32-42 are unused
	uint8_t always0 : 1; ///< 2/11 aka 43 is always 0
	uint8_t always1 : 1; ///< 2/12 aka 44 is always 1
	uint8_t dpl : 2; ///< 2/13-14 aka 45-46 are unused, page dpl is used for data see also memory_page_entry_t
	uint8_t present : 1; ///< 2/15 aka 47 is always 1
	uint16_t unused3; ///< 2/16-31 aka 48-63 are unused
}__attribute__((packed)) descriptor_gdt_data_t; ///< struct short hand

/**
 * @struct descriptor_gdt
 * @brief  general segment descriptor for gdt
 *
 * only works for null, code, data. tss should be handled specially
 */
typedef struct descriptor_gdt {
	union {
		descriptor_gdt_null_t null; ///< null segment
		descriptor_gdt_code_t code; ///< code segment
		descriptor_gdt_data_t data; ///< data segment
	};
}__attribute__((packed)) descriptor_gdt_t; ///< struct short hand

/**
 * @brief null segment builder macro
 * @param  seg segment to build as null
 */
#define DESCRIPTOR_BUILD_GDT_NULL_SEG(seg) {seg.null.unused1 = 0; seg.null.unused2 = 0;}

/**
 * @brief code segment builder macro with privilage level
 * @param  seg segment to build as code
 * @param  DPL privilage level to build as code
 */
#define DESCRIPTOR_BUILD_GDT_CODE_SEG(seg, DPL) {seg.code.unused1 = 0; \
		                                             seg.code.unused2 = 0; \
		                                             seg.code.conforming = 0; \
		                                             seg.code.always1 = 3; \
		                                             seg.code.dpl = DPL; \
		                                             seg.code.present = 1; \
		                                             seg.code.unused3 = 0; \
		                                             seg.code.avl = 0; \
		                                             seg.code.long_mode = 1; \
		                                             seg.code.default_opsize = 0; \
		                                             seg.code.unused4 = 0;}

/**
 * @brief data segment builder macro
 * @param  seg segment to build as data
 */
#define DESCRIPTOR_BUILD_GDT_DATA_SEG(seg) {seg.data.unused1 = 0; \
		                                        seg.data.unused2 = 0; \
		                                        seg.data.always0 = 0; \
		                                        seg.data.always1 = 1; \
		                                        seg.data.dpl = 0; \
		                                        seg.data.present = 1; \
		                                        seg.data.unused3 = 0;}
#define IDT_BASE_ADDRESS (0)
/*! IDT segment type */
#define SYSTEM_SEGMENT_TYPE_IDT           0x02
/*! Active TSS segment type */
#define SYSTEM_SEGMENT_TYPE_TSS_A         0x09
/*! Busy TSS segment type */
#define SYSTEM_SEGMENT_TYPE_TSS_B         0x0B
/*! Call Gate segment type */
#define SYSTEM_SEGMENT_TYPE_GATE_CALL     0x0C
/*! Interrupt Gate segment type */
#define SYSTEM_SEGMENT_TYPE_GATE_INT      0x0E
/*! Trap Gate segment type */
#define SYSTEM_SEGMENT_TYPE_GATE_TRAP     0x0F

/**
 * @struct descriptor_idt
 * @brief  idt table descriptor
 *
 * each table member has this struct format for interrupt function
 */
typedef struct descriptor_idt {
	uint16_t offset_1;  ///< offset bits 0..15 of where interrupt function reside
	uint16_t selector;  ///< a code segment selector in GDT or LDT
	uint8_t ist : 3;    ///< bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
	uint8_t always0_1 : 5; ///< ignore, always 0
	uint8_t type : 4;  ///< interrupt type type it should be 0xE
	uint8_t always0_2 : 1; ///< ignore, always 0
	uint8_t dpl : 2; ///< privilage level of interrupt
	uint8_t present : 1; ///< interrupt present?
	uint16_t offset_2;  ///< offset bits 16..31 of where interrupt function reside
	uint32_t offset_3;  ///< offset bits 32..63 of where interrupt function reside
	uint32_t zero;      ///< reserved, always zero
}__attribute__((packed)) descriptor_idt_t; ///< struct short hand

/**
 * @brief assign a function to interrupt nomber
 * @param[in]  IE        interrupt vector
 * @param[in]  FUNC_ADDR interrupt vector's function address
 * @param[in]  SELECTOR  gdt code selector for interrupt vector
 * @param[in]  IST       ist number for vector, if ist is zero not ist mechanism, else use ist at tss
 * @param[in]  DPL       privilage level of interrupt
 */
#define DESCRIPTOR_BUILD_IDT_SEG(IE, FUNC_ADDR, SELECTOR, IST, DPL) { \
		IE.selector = SELECTOR; \
		IE.ist = IST; \
		IE.always0_1 = 0; \
		IE.type = 0xE; \
		IE.always0_2 = 0; \
		IE.dpl = DPL; \
		IE.present = 1; \
		IE.zero = 0; \
		IE.offset_1 = (FUNC_ADDR >> 0) & 0xFFFF; \
		IE.offset_2 = (FUNC_ADDR >> 16) & 0xFFFF; \
		IE.offset_3 = (FUNC_ADDR >> 32) & 0xFFFFFFFF; \
}

/**
 * @struct descriptor_register
 * @brief  common register value for gdtr, idtr, ldtr, tr
 *
 */
typedef struct descriptor_register {
	uint16_t limit; ///< size of table-1
	uint64_t base; ///< where is table?
}__attribute__((packed)) descriptor_register_t; ///< struct short hand

/**
 * @brief static address of gdt for lgdtr/sgdtr
 */
extern descriptor_register_t* GDT_REGISTER;

/**
 * @brief static address of idt for lidtr/sidtr
 */
extern descriptor_register_t* IDT_REGISTER;

/**
 * @brief builds a default gdt
 * @return 0 at success.
 */
uint8_t descriptor_build_gdt_register();

/**
 * @brief builds a default idt
 * @return 0 at success.
 */
uint8_t descriptor_build_idt_register();

#endif
