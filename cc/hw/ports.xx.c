/**
 * @file ports.xx.c
 * @brief port communication
 *
 * the functions inside this file are for port communication. the in/out functions
 * defined at header file as inline functions.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <ports.h>

/**
 * @brief checks data present at port
 * @param[in]  port port to check
 * @return  0 if no data, else 1
 *
 * it uses control port offset at +5 to check data.
 */
uint8_t serial_received(uint16_t port);

/**
 * @brief checks data can be sended
 * @param[in]  port port to check
 * @return  0 if data can be sended, else 1
 *
 * it uses control port offset at +5 to check data can be sended.
 */
uint8_t is_transmit_empty(uint16_t port);

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

uint8_t read_serial(uint16_t port) {
	while (serial_received(port) == 0);

	return inb(port);
}

uint8_t is_transmit_empty(uint16_t port) {
	return inb(port + 5) & 0x20;
}

void write_serial(uint16_t port, uint8_t data) {
	while (is_transmit_empty(port) == 0);

	outb(port, data);
}
