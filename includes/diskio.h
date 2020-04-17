#ifndef ___DISKIO_H
#define ___DISKIO_H 0

#include <types.h>

extern unsigned char BOOT_DRIVE;


typedef struct lbasupport {
	uint8_t majorver;
	uint8_t minorver;
	uint8_t api;
} __attribute__ ((packed)) lbasupport_t;

typedef struct disk_slot {
	uint8_t type;
	uint8_t unused[7];
	uint64_t start;
	uint64_t end;
} __attribute__ ((packed)) disk_slot_t;

typedef struct disk_slot_table {
	disk_slot_t slots[10];
} __attribute__ ((packed)) disk_slot_table_t;

uint8_t disk_check_lba_support(lbasupport_t*);
uint16_t disk_read(uint64_t, uint16_t,uint8_t**);
uint16_t disk_read_slottable(disk_slot_table_t **);

#endif
