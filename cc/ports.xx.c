#include <ports.h>

uint8_t serial_received(uint16_t);
uint8_t is_transmit_empty(uint16_t);

void init_serial(uint16_t port) {
	outb(port + 1, 0x00);
	outb(port + 3, 0x80);
	outb(port + 0, 0x01);
	outb(port + 1, 0x00);
	outb(port + 3, 0x03);
	outb(port + 2, 0xC7);
	outb(port + 4, 0x0B);
}

uint8_t serial_received(uint16_t port) {
	return inb(port + 5) & 1;
}

char_t read_serial(uint16_t port) {
	while (serial_received(port) == 0);

	return inb(port);
}

uint8_t is_transmit_empty(uint16_t port) {
	return inb(port + 5) & 0x20;
}

void write_serial(uint16_t port, char_t data) {
	while (is_transmit_empty(port) == 0);

	outb(port, data);
}
