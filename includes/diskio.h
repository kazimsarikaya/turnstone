#ifndef ___DISKIO
#define ___DISKIO 0

extern unsigned char BOOT_DRIVE;


typedef struct _lbasupport {
	char majorver;
	char minorver;
	char api;
} lbasupport;

int check_lba_support(lbasupport*);

#endif
