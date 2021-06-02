/**
 * @file cpu.64.c
 * @brief 64 bit cpu operations
 */

#include <cpu.h>

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
