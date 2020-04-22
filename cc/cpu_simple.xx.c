#include <cpu.h>

typedef struct cpu_cpuid_regs {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
} cpu_cpuid_regs_t;

uint8_t cpu_check_cpuid();
uint8_t cpu_cpuid(cpu_cpuid_regs_t, cpu_cpuid_regs_t*);

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

uint8_t cpu_check_longmode() {
#if ___BITS == 16
	if(cpu_check_cpuid() != 0) {
		return -1;
	}
	cpu_cpuid_regs_t query = {0x80000001, 0, 0, 0};
	cpu_cpuid_regs_t answer = {0, 0, 0, 0};
	if(cpu_cpuid(query, &answer) != 0) {
		return -1;
	}
	if((answer.edx & 0x20000800) == 0x20000800) {
		return 0;
	}
	return -1;
#elif ___BITS == 64
	return 0;
#endif
}


uint8_t cpu_cpuid(cpu_cpuid_regs_t query, cpu_cpuid_regs_t* answer){
	__asm__ __volatile__ ("cpuid\n"
	                      : "=a" (answer->eax), "=b" (answer->ebx), "=c" (answer->ecx), "=d" (answer->edx)
	                      : "a" (query.eax), "b" (query.ebx), "c" (query.ecx), "d" (query.edx)
	                      );
	if(answer->eax == 0 && answer->ebx == 0 && answer->ecx == 0 && answer->edx == 0) {
		return -1;
	}
	return 0;
}

uint8_t cpu_check_cpuid() {
#if ___BITS == 16
	uint8_t res;
	__asm__ __volatile__ ("pushf\n"
	                      "pop    %%eax\n"
	                      "mov    %%eax, %%ecx\n"
	                      "xor    $0x00200000, %%eax\n"
	                      "push   %%eax\n"
	                      "popf\n"
	                      "pushf\n"
	                      "pop    %%eax\n"
	                      "xor    %%ecx, %%eax\n"
	                      "shr    $0x15, %%eax\n"
	                      "and    $0x1, %%eax\n"
	                      "push   %%ecx\n"
	                      "popf\n"
	                      "test   %%eax, %%eax\n"
	                      "jz .nocpuid\n"
	                      "mov $0x80000000, %%eax\n"
	                      "cpuid\n"
	                      "cmp $0x80000001, %%eax\n"
	                      "jb .nocpuid\n"
	                      "mov $0x0, %%eax\n"
	                      "jmp .cpuidavail\n"
	                      ".nocpuid: mov $0x1, %%eax\n"
	                      ".cpuidavail:\n"
	                      : "=a" (res)
	                      );
	return res;
#elif ___BITS == 64
	return 0;
#endif
}
