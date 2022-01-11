/**
 * @file cpu.64.c
 * @brief 64 bit cpu operations
 */

#include <cpu.h>
#include <cpu/crx.h>

uint64_t cpu_read_msr(uint32_t msr_address){
	uint64_t low_part, high_part;
	__asm__ __volatile__ ("rdmsr" : "=a" (low_part), "=d" (high_part) : "c" (msr_address));
	return (high_part << 32) | low_part;
}

int8_t cpu_write_msr(uint32_t msr_address, uint64_t value) {
	uint64_t low_part = (value & 0xFFFFFFFF), high_part = ((value >> 32) & 0xFFFFFFFF);
	__asm__ __volatile__ ("wrmsr" : : "a" (low_part), "d" (high_part), "c" (msr_address));
	return 0;
}

uint64_t cpu_read_cr2() {
	uint64_t cr2 = 0;
	__asm__ __volatile__ ("mov %%cr2, %0\n"
	                      : "=r" (cr2));
	return cr2;
}


cpu_reg_cr4_t cpu_read_cr4() {
	cpu_reg_cr4_t cr4;
	__asm__ __volatile__ ("mov %%cr4, %0\n"
	                      : "=r" (cr4));
	return cr4;
}

void cpu_write_cr4(cpu_reg_cr4_t cr4) {
	__asm__ __volatile__ ("mov %0, %%cr4\n"
	                      : : "r" (cr4));
}


cpu_reg_cr0_t cpu_read_cr0() {
	cpu_reg_cr0_t cr0;
	__asm__ __volatile__ ("mov %%cr0, %0\n"
	                      : "=r" (cr0));
	return cr0;
}

void cpu_write_cr0(cpu_reg_cr0_t cr0) {
	__asm__ __volatile__ ("mov %0, %%cr0\n"
	                      : : "r" (cr0));
}

void cpu_toggle_cr0_wp() {
	cpu_reg_cr0_t cr0 = cpu_read_cr0();

	cr0.write_protect = ~cr0.write_protect;

	cpu_write_cr0(cr0);
}
