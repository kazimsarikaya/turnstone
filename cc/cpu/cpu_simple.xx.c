/**
 * @file cpu_simple.xx.c
 * @brief cpu commands implementation that supports both real and long mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <cpu.h>
#include <video.h>

/**
 * @brief check cpuid is supported by cpu.
 * @return 0 if cpuid supported.
 */
uint8_t cpu_check_cpuid();

/**
 * @brief performs cpuid query.
 * @param[in] query registers.
 * @param[out] answer registers.
 * @return 0 if method succeed.
 */
uint8_t cpu_cpuid(cpu_cpuid_regs_t, cpu_cpuid_regs_t*);

void cpu_hlt() {
	for(;;) {
		cpu_cli();
		__asm__ __volatile__ ("hlt");
	}
}

void cpu_idle() {
	__asm__ __volatile__ ("hlt");
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

int8_t cpu_check_rdrand(){
	cpu_cpuid_regs_t query = {0x80000001, 0, 0, 0};
	cpu_cpuid_regs_t answer = {0, 0, 0, 0};
	if(cpu_cpuid(query, &answer) != 0) {
		return -1;
	}
	if((answer.ecx & 0x40000000) == 0x40000000) {
		return 0;
	}
	return -1;
}


uint8_t cpu_cpuid(cpu_cpuid_regs_t query, cpu_cpuid_regs_t* answer){
	__asm__ __volatile__ ("cpuid\n"
	                      : "=a" (answer->eax), "=b" (answer->ebx), "=c" (answer->ecx), "=d" (answer->edx)
	                      : "a" (query.eax), "b" (query.ebx), "c" (query.ecx), "d" (query.edx)
	                      );
	if(answer->eax == 0 && answer->ebx == 0 && answer->ecx == 0 && answer->edx == 0) {
		printf("CPU: Fatal cpuid failed\n");

		return -1;
	}

	return 0;
}
