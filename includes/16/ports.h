#ifndef ___PORTS_H
#define ___PORTS_H 0

#define COM1    0x03F8

static inline void outb(unsigned short port, unsigned char data) {
	asm volatile ( "outb %0, %1" : : "a" (data), "dN" (port));
}

static inline unsigned char inb(unsigned short port){
	unsigned char ret;
	asm volatile ("inb %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

#endif
