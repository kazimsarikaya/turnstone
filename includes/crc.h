#ifndef ___CRC_H
#define ___CRC_H 0

#include <types.h>

#define CRC32_SEED  0xffffffff

void crc32_init_table();
uint32_t crc32_sum(uint8_t* p, uint32_t bytelength, uint32_t init);

#endif
