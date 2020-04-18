#include <descriptor.h>
#include <memory.h>

uint8_t descriptor_build_gdt_register(){
	uint16_t gdt_size = sizeof(descriptor_gdt_t)*3;
	descriptor_gdt_t *gdts = simple_kmalloc(gdt_size);
	if(gdts==NULL) {
		return -1;
	}
	BUILD_GDT_NULL_SEG(gdts[0]);
	BUILD_GDT_CODE_SEG(gdts[1],DPL_KERNEL);
	BUILD_GDT_DATA_SEG(gdts[2]);
	GDT_REGISTER.limit = gdt_size -1;
#if ___BITS == 16
	GDT_REGISTER.base.part_low = get_absolute_address((uint32_t)gdts);
	GDT_REGISTER.base.part_high = 0;
#endif
	return 0;
}

uint8_t descriptor_build_idt_register(){
	uint16_t idt_size = sizeof(descriptor_idt_t)*256;
	IDT_REGISTER.limit = idt_size-1;
#if ___BITS == 16
	IDT_REGISTER.base.part_low = 0x100000;
	IDT_REGISTER.base.part_high = 0;
#endif
	return 0;
}
