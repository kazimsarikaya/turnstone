/**
 * @file ports.h
 * @brief i/o port functions
 *
 * this functions uses inline assembly
 */
#ifndef ___PORTS_H
/*! prevent duplicate header error macro */
#define ___PORTS_H 0

#include <types.h>

/**
 * COM1 value
 */
#define COM1    0x03F8

/**
 * @brief write one byte to the port
 * @param[in] port port to write
 * @param[in] data one byte data to write
 */
static inline void outb(uint16_t port, uint8_t data) {
	asm volatile ( "outb %0, %1" : : "a" (data), "dN" (port));
}

/**
 * @brief read one byte from the port
 * @param[in] port port to write
 * @return one byte data
 */
static inline uint8_t inb(uint16_t port){
	uint8_t ret;
	asm volatile ("inb %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

/**
 * @brief write one word (two bytes) to the port
 * @param[in] port port to write
 * @param[in] data one word (two bytes) data to write
 */
static inline void outw(uint16_t port, uint16_t data) {
	asm volatile ( "outw %0, %1" : : "a" (data), "dN" (port));
}

/**
 * @brief read one word (two bytes) from the port
 * @param[in] port port to write
 * @return one word (two bytes) data
 */
static inline uint16_t inw(uint16_t port){
	uint16_t ret;
	asm volatile ("inw %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

/**
 * @brief write double word (four bytes) to the port
 * @param[in] port port to write
 * @param[in] data double word (four bytes) data to write
 */
static inline void outl(uint16_t port, uint32_t data) {
	asm volatile ( "outl %0, %1" : : "a" (data), "dN" (port));
}

/**
 * @brief read double word (four bytes) from the port
 * @param[in] port port to write
 * @return double word (four bytes) data
 */
static inline uint32_t inl(uint16_t port){
	uint32_t ret;
	asm volatile ("inl %1, %0" : "=a" (ret) : "d" (port));
	return ret;
}

/**
 * @brief setups serial port
 * @param[in] port port to setups
 */
void init_serial(uint16_t port);

/**
 * @brief reads one byte from serial port
 * @param[in]  port port to read
 * @return readed value
 */
uint8_t read_serial(uint16_t port);

/**
 * @brief writes one byte to serial port
 * @param[in]  port port to write
 * @param[in] data data value to be writen
 */
void write_serial(uint16_t port, uint8_t data);

#endif
