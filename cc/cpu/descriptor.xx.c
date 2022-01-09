#include <types.h>
#include <cpu/descriptor.h>
#include <memory.h>
#include <memory/frame.h>
#include <systeminfo.h>

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

	__asm__ __volatile__ ("lgdt (%%rax)\n"
	                      "push $0x08\n"
	                      "lea fix_gdt_jmp%=(%%rip),%%rax\n"
	                      "push %%rax\n"
	                      "lretq\n"
	                      "fix_gdt_jmp%=:"
	                      : : "a" (GDT_REGISTER));
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

	if(SYSTEM_INFO->remapped == 0) {
		frame_t idt_frame = {IDT_BASE_ADDRESS, 1, FRAME_TYPE_RESERVED, 0};
		KERNEL_FRAME_ALLOCATOR->allocate_frame(KERNEL_FRAME_ALLOCATOR, &idt_frame);
	}

	IDT_REGISTER->limit = idt_size - 1;
	IDT_REGISTER->base = IDT_BASE_ADDRESS;

	__asm__ __volatile__ ("lidt (%%rax)\n" : : "a" (IDT_REGISTER));

	return 0;
}
