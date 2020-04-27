/**
 * @file diskio.16.c
 * @brief diskio at real mode
 *
 * reading disk at real mode is performed by bios interrupts. BIOS handles ATA or SATA disks.
 * The used interrupt is int 0x13,0x42
 */
#include <types.h>
#include <diskio.h>
#include <memory.h>
#include <cpu.h>

/**
 * @struct disk_bios_dap
 * @brief  disk read interrupt needs this struct
 */
typedef struct disk_bios_dap {
	uint8_t size; ///< dap size always 0x10
	uint8_t unused; ///< unused data
	uint16_t sector_count; ///< how many sectors will be readed
	uint16_t offset; ///< the offset of readed or will be written data
	uint16_t segment; ///< the segment of readed or will be written data
	uint64_t lba; ///< start lba address
}__attribute__((packed)) disk_bios_dap_t; ///< short hand for struct

uint16_t disk_read(uint8_t hard_disk, uint64_t lba, uint16_t sector_count, uint8_t** data){
	disk_bios_dap_t* dap;
	uint16_t status, err;

	*data = memory_malloc(sizeof(uint8_t) * sector_count * 512);
	if(*data == NULL) {
		return -1;
	}
	dap = memory_malloc(sizeof(disk_bios_dap_t));
	if(dap == NULL) {
		memory_free(*data);
		return -2;
	}
	dap->size = 0x10;
	dap->unused = 0;
	dap->sector_count = sector_count;
	dap->offset = (uint16_t)(((uint32_t)*data) & 0xFFFF);
	dap->segment = cpu_read_data_segment();
	dap->lba.part_low = lba.part_low;
	dap->lba.part_high = lba.part_high;

	__asm__ __volatile__ ("int $0x13\n"
	                      : "=@ccc" (err), "=a" (status)
	                      : "a" (0x4200), "d" (hard_disk), "S" (dap)
	                      );

	memory_free(dap);
	if(err != 0) {
		memory_free(*data);
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
	if(status == 0) {
		*pst = memory_malloc(sizeof(disk_slot_table_t));
		memory_memcopy(data + 0x110, (uint8_t*)*pst, 0xF0);
	}
	memory_free(data);
	return status;
}
