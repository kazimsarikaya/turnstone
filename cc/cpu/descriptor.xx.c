#include <types.h>
#include <cpu/descriptor.h>
#include <memory.h>

uint8_t descriptor_build_gdt_register(){
	uint16_t gdt_size = sizeof(descriptor_gdt_t) * 5;
	descriptor_gdt_t* gdts = memory_malloc(gdt_size);
	if(gdts == NULL) {
		return -1;
	}
	DESCRIPTOR_BUILD_GDT_NULL_SEG(gdts[0]);
	DESCRIPTOR_BUILD_GDT_CODE_SEG(gdts[1], DPL_KERNEL);
	DESCRIPTOR_BUILD_GDT_DATA_SEG(gdts[2]);
	GDT_REGISTER.limit = gdt_size - 1;
	GDT_REGISTER.base = (size_t)gdts;
	return 0;
}

uint8_t descriptor_build_idt_register(){
	uint16_t idt_size = sizeof(descriptor_idt_t) * 256;
	IDT_REGISTER.limit = idt_size - 1;
	IDT_REGISTER.base = IDT_BASE_ADDRESS;
	return 0;
}
