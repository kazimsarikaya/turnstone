#ifndef ___DISKIO_H
#define ___DISKIO_H 0

extern unsigned char BOOT_DRIVE;


typedef struct _lbasupport {
	char majorver;
	char minorver;
	char api;
} __attribute__ ((packed)) lbasupport;

int check_lba_support(lbasupport*);

#endif
