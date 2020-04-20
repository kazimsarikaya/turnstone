#include <types.h>
#include <diskio.h>
#include <memory.h>
#include <cpu.h>

typedef struct disk_bios_dap {
	uint8_t size;
	uint8_t unused;
	uint16_t sector_count;
	uint16_t offset;
	uint16_t segment;
	uint64_t lba;
}__attribute__((packed)) disk_bios_dap_t;

uint16_t disk_read(uint8_t hard_disk, uint64_t lba, uint16_t sector_count, uint8_t** data){
	disk_bios_dap_t* dap;
	uint16_t status, err;

	*data = memory_simple_kmalloc(sizeof(uint8_t) * sector_count * 512);
	if(*data==NULL) {
		return -1;
	}
	dap = memory_simple_kmalloc(sizeof(disk_bios_dap_t));
	if(dap==NULL) {
		memory_simple_kfree(*data);
		return -2;
	}
	dap->size = 0x10;
	dap->unused = 0;
	dap->sector_count = sector_count;
	dap->offset = (uint16_t)(((uint32_t)*data) & 0xFFFF);
	dap->segment = cpu_read_data_segment();
	dap->lba.part_low = lba.part_low;
	dap->lba.part_high = lba.part_high;

	__asm__ __volatile__ ( "int $0x13"
	                       : "=@ccc" (err), "=a" (status)
	                       : "a" (0x4200), "d" (hard_disk), "S" (dap)
	                       );

	memory_simple_kfree(dap);
	if(err!=0) {
		memory_simple_kfree(*data);
	}
	return status;
}


uint16_t disk_read_slottable(uint8_t hard_disk, disk_slot_table_t** pst){
#if ___BITS == 16
	uint64_t lba = {1, 0};
#elif ___BITS == 64
	uint64_t lba = 1;
#endif
	uint8_t* data;
	uint16_t status = disk_read(hard_disk, lba, 1, &data);
	if(status==0) {
		*pst = memory_simple_kmalloc(sizeof(disk_slot_table_t));
		memory_simple_memcpy(data + 0x110, (uint8_t*)*pst, 0xF0);
	}
	memory_simple_kfree(data);
	return status;
}
