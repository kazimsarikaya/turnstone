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
#include <video.h>

disk_geometry_t* disk_geometry = NULL;

int8_t disk_cache_geometry(uint8_t hard_disk) {
	if(disk_geometry != NULL) {
		return 0;
	}

	disk_geometry = memory_malloc(sizeof(disk_geometry_t));
	if(disk_geometry == NULL) {
		return -1;
	}

	uint16_t ax, cx, dx;

	__asm__ __volatile__ ("push %%es\n"
	                      "mov    %%bx, %%es\n"
	                      "int    $0x13\n"
	                      "pop %%es\n"
	                      : "=a" (ax), "=c" (cx), "=d" (dx)
	                      : "a" (0x0800), "b" (0), "d" (hard_disk), "D" (0)
	                      );

	if((ax & 0xFF00) != 0) {
		return ax >> 8;
	}

	disk_geometry->head_count = (dx >> 8) + 1;
	disk_geometry->sector_count = cx & 0x3F;
	disk_geometry->cylinder_count = (cx >> 6) + 1;

	printf("DISKIO: CYL %i HEAD %i SEC %i\n", disk_geometry->cylinder_count,  disk_geometry->head_count, disk_geometry->sector_count );

	return 0;
}

uint16_t disk_read(uint8_t hard_disk, uint64_t lba, uint16_t sector_count, uint8_t** data){
	uint16_t status, err;

	if(disk_cache_geometry(hard_disk) != 0) {
		return -1;
	}

	*data = memory_malloc(sizeof(uint8_t) * sector_count * 512);
	if(*data == NULL) {
		return -1;
	}

	uint16_t ax, cx, dx;

	ax = 0x0200 | sector_count;

	dx = (lba.part_low / disk_geometry->sector_count) % disk_geometry->head_count;
	dx = (dx << 8) | hard_disk;

	cx = lba.part_low / (disk_geometry->sector_count * disk_geometry->head_count);
	cx = (cx << 6) | ((lba.part_low % disk_geometry->sector_count) + 1);

	__asm__ __volatile__ ("int $0x13\n"
	                      : "=@ccc" (err), "=a" (status)
	                      : "a" (ax), "c" (cx), "d" (dx), "b" (*data)
	                      );

	if(err != 0) {
		memory_free(*data);
		status >>= 8;
	} else {
		status = 0;
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
