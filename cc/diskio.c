asm (".code16gcc");

#include <diskio.h>

int check_lba_support(lbasupport *ls){
	char status = 0;
	char api = 0, code = 0, minver =0;
	int ver = 0;

	asm volatile (
		"int $0x13\n"
		"mov %%ah, %[code]\n"
		"mov %%cl, %[api]\n"
		"mov %%dh, %[minver]\n"
		"setc %%bl\n"
		"mov %%bl, %[status]\n"
		: [code] "=a" (code), [api] "=c" (api), [minver] "=d" (minver),  [status] "=b" (status)
		: "a" (0x4100), "b" (0x55aa), "d" (BOOT_DRIVE)
		);

	if(status == 0) {
		ls->majorver = code;
		ls->minorver = minver;
		ls->api = api;
	}

	return status;
}
