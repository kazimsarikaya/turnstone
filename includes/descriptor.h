#ifndef ___DESCRIPTOR
#define ___DESCRIPTOR 0

#include <types.h>

#define DPL_RING0 0
#define DPL_RING1 1
#define DPL_RING2 2
#define DPL_RING3 3

#define DPL_KERNEL DPL_RING0
#define DPL_USER   DPL_RING3

typedef struct descriptor_gdt_code {
	uint32_t unused1; // 1/0-31
	uint16_t unused2 : 10;  // 2/0-9 aka 32-41
	uint8_t conforming : 1; // 2/10 aka 42
	uint8_t always1 : 2; // 2/11-12 aka 43-44
	uint8_t dpl : 2; // 2/13-14 aka 45-46
	uint8_t present : 1; // 2/15 aka 47
	uint8_t unused3 : 4; // 2/16-19 aka 48-51
	uint8_t avl : 1; // 2/20 aka 52
	uint8_t long_mode : 1; // 2/21 aka 53
	uint8_t default_opsize : 1; // 2/22 aka 54
	uint8_t unused4 : 1; // 2/23 aka 55
	uint8_t unused5; // 2/24-31 aka 56-63

}__attribute__((packed)) descriptor_gdt_code_t;

#define BUILD_GDT_CODE_SEG(seg,DPL) {seg.code.unused1=0; \
		                                 seg.code.unused2=0; \
		                                 seg.code.conforming=0; \
		                                 seg.code.always1=3; \
		                                 seg.code.dpl=DPL; \
		                                 seg.code.present=1; \
		                                 seg.code.unused3=0; \
		                                 seg.code.avl=0; \
		                                 seg.code.long_mode=1; \
		                                 seg.code.default_opsize=0; \
		                                 seg.code.unused4=0;}

typedef struct descriptor_gdt_data {
	uint32_t unused1; // 1/0-31
	uint16_t unused2 : 11;  // 2/0-10 aka 32-42
	uint8_t always0 : 1; // 2/11 aka 43
	uint8_t always1 : 1; // 2/12 aka 44
	uint8_t dpl : 2; // 2/13-14 aka 45-46
	uint8_t present : 1; // 2/15 aka 47
	uint16_t unused3; // 2/16-31 aka 48-63
}__attribute__((packed)) descriptor_gdt_data_t;

#define BUILD_GDT_DATA_SEG(seg) {seg.data.unused1=0; \
		                             seg.data.unused2=0; \
		                             seg.data.always0=0; \
		                             seg.data.always1=1; \
		                             seg.data.dpl=0; \
		                             seg.data.present=1; \
		                             seg.data.unused3=0;}

typedef struct descriptor_gdt_null {
	uint32_t unused1;
	uint32_t unused2;
}__attribute__((packed)) descriptor_gdt_null_t;

#define BUILD_GDT_NULL_SEG(seg) {seg.null.unused1=0; seg.null.unused2=0;}

typedef struct descriptor_gdt {
	union {
		descriptor_gdt_null_t null;
		descriptor_gdt_code_t code;
		descriptor_gdt_data_t data;
	};
}__attribute__((packed)) descriptor_gdt_t;

typedef struct descriptor_idt {
	uint16_t offset_1;  // offset bits 0..15
	uint16_t selector;  // a code segment selector in GDT or LDT
	uint8_t ist;        // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
	uint8_t type_attr;  // type and attributes
	uint16_t offset_2;  // offset bits 16..31
	uint32_t offset_3;  // offset bits 32..63
	uint32_t zero;      // reserved
}__attribute__((packed)) descriptor_idt_t;


typedef struct descriptor_register {
	uint16_t limit;
	uint64_t base;
}__attribute__((packed)) descriptor_register_t;


extern descriptor_register_t GDT_REGISTER;
extern descriptor_register_t IDT_REGISTER;

uint8_t descriptor_build_gdt_register();
uint8_t descriptor_build_idt_register();

#endif
