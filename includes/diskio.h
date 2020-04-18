#ifndef ___DISKIO_H
#define ___DISKIO_H 0

#include <types.h>

typedef struct disk_slot {
	uint8_t type;
	uint8_t unused[7];
	uint64_t start;
	uint64_t end;
} __attribute__ ((packed)) disk_slot_t;

typedef struct disk_slot_table {
	disk_slot_t slots[10];
} __attribute__ ((packed)) disk_slot_table_t;

uint16_t disk_read(uint64_t, uint16_t,uint8_t**);
uint16_t disk_read_slottable(disk_slot_table_t **);

#endif
