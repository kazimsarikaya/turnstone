#ifndef ___CPU
#define ___CPU 0

#include <types.h>

void cpu_hlt();
void cpu_cli();
void cpu_sti();
uint16_t cpu_read_data_segment();
uint8_t cpu_check_longmode();

#endif
