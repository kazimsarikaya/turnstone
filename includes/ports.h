#ifndef ___PORTS_H
#define ___PORTS_H 0

#include <types.h>

#define COM1    0x03F8

static inline void outb(uint16_t port, uint8_t data) {
	asm volatile ( "outb %0, %1" : : "a" (data), "dN" (port));
}

static inline uint8_t inb(uint16_t port){
	uint8_t ret;
	asm volatile ("inb %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

#endif
