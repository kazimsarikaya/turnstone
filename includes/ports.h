#ifndef ___PORTS_H
#define ___PORTS_H 0

#include <types.h>

#define COM1    0x03F8
#define PIO_MASTER  0x01F0

static inline void outb(uint16_t port, uint8_t data) {
	asm volatile ( "outb %0, %1" : : "a" (data), "dN" (port));
}

static inline uint8_t inb(uint16_t port){
	uint8_t ret;
	asm volatile ("inb %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

static inline void outw(uint16_t port, uint16_t data) {
	asm volatile ( "outw %0, %1" : : "a" (data), "dN" (port));
}

static inline uint16_t inw(uint16_t port){
	uint16_t ret;
	asm volatile ("inw %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

void init_serial(uint16_t);
char_t read_serial(uint16_t);
void write_serial(uint16_t, char_t);

#endif
