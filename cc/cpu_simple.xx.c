#include <cpu.h>

void cpu_hlt() {
	for(;;) {
		cpu_cli();
		__asm__ __volatile__ ("hlt");
	}
}

void cpu_cli() {
	__asm__ __volatile__ ("cli");
}

void cpu_sti() {
	__asm__ __volatile__ ("sti");
}


uint16_t cpu_read_data_segment(){
	uint16_t ds;
	__asm__ __volatile__ ("mov %%ds, %0" : "=r" (ds));
	return ds;
}
