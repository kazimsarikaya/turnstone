#ifndef ___DISKIO_H
#define ___DISKIO_H 0

#include <types.h>

extern unsigned char BOOT_DRIVE;


typedef struct _lbasupport {
	uint8_t majorver;
	uint8_t minorver;
	uint8_t api;
} __attribute__ ((packed)) lbasupport;

int check_lba_support(lbasupport*);

#endif
