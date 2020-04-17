#include <diskio.h>
#include <memory.h>
#include <ports.h>

uint16_t pool_drive();

uint8_t disk_check_lba_support(lbasupport_t *ls){
	uint8_t status = 0;
	uint8_t api = 0, code = 0, minver =0;

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

uint16_t disk_read(uint64_t lba, uint16_t sector_count, uint8_t** data){
	if(sector_count==0) {
		return 1;
	}
	*data=simple_kmalloc(sizeof(uint8_t)*sector_count*512);
	if(*data==NULL) {
		return 2;
	}
	outb(PIO_MASTER+6,0x40);
	outb(PIO_MASTER+2, (sector_count>>8)&0xFF);
	outb(PIO_MASTER+3, (lba.part_high>>0)&0xFF);
	outb(PIO_MASTER+4, (lba.part_high>>8)&0xFF);
	outb(PIO_MASTER+5, (lba.part_high>>16)&0xFF);
	outb(PIO_MASTER+2, (sector_count>>0)&0xFF);
	outb(PIO_MASTER+3, (lba.part_low>>0)&0xFF);
	outb(PIO_MASTER+4, (lba.part_low>>8)&0xFF);
	outb(PIO_MASTER+5, (lba.part_low>>16)&0xFF);
	outb(PIO_MASTER+7, 0x24);
	uint8_t status;
	uint16_t *t_data=(uint16_t*)*data;
	uint32_t k=0;
	for(uint16_t i =0; i< sector_count; i++) {
		status = pool_drive();
		if(status==0) {
			for(uint16_t j=0; j<256; j++) {
				t_data[k++] = inw(PIO_MASTER);
			}
		} else {
			simple_kfree(*data);
			return status;
		}
	}

	return 0;
}


uint16_t disk_read_slottable(disk_slot_table_t **pst){
	uint64_t lba = {1,0};
	uint8_t *data;
	uint16_t status = disk_read(lba,1,&data);
	if(status==0) {
		*pst=simple_kmalloc(sizeof(disk_slot_t));
		simple_memcpy(&data[0x110],(uint8_t*)*pst,0xF0);
	}
	return status;
}

uint16_t pool_drive() {
	uint8_t status;
	do {
		status = inb(PIO_MASTER+7);
		if((status & 0x80) == 0x80) { //BSY
			continue;
		}
		if((status & 0x08 )== 0x08) { //DRQ
			break;
		}
		if((status & 0x01 )== 0x01) { //ERR
			uint16_t error = inw(PIO_MASTER+1);
			return error | 0xFF00;
		}
		if((status & 0x20) == 0x20) { //DF
			return 1;
		}
		if((status & 0x40) ==  0x40) { //RDY
			break;
		}
	}while(1);
	return 0;
}
