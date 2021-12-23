#include <types.h>
#include <cpu/descriptor.h>
#include <memory.h>

descriptor_register_t* GDT_REGISTER = NULL;
descriptor_register_t* IDT_REGISTER = NULL;

uint8_t descriptor_build_gdt_register(){
	uint16_t gdt_size = sizeof(descriptor_gdt_t) * 5;
	descriptor_gdt_t* gdts = memory_malloc(gdt_size);
	if(gdts == NULL) {
		return -1;
	}
	DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
	DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
	DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2]);

	if(GDT_REGISTER) {
		memory_free(GDT_REGISTER);
	}

	GDT_REGISTER = memory_malloc(sizeof(descriptor_register_t));
	if(GDT_REGISTER == NULL) {
		return -1;
	}

	GDT_REGISTER->limit = gdt_size - 1;
	GDT_REGISTER->base = (size_t)gdts;

	asm volatile ("lgdt (%%rax)\n" : : "a" (GDT_REGISTER));
	return 0;
}

uint8_t descriptor_build_idt_register(){
	uint16_t idt_size = sizeof(descriptor_idt_t) * 256;

	if(IDT_REGISTER) {
		memory_free(IDT_REGISTER);
	}

	IDT_REGISTER = memory_malloc(sizeof(descriptor_register_t));
	if(IDT_REGISTER == NULL) {
		return -1;
	}

	IDT_REGISTER->limit = idt_size - 1;
	IDT_REGISTER->base = IDT_BASE_ADDRESS;

	asm volatile ("lidt (%%rax)\n" : : "a" (IDT_REGISTER));

	return 0;
}
