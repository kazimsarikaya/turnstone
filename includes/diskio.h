/**
 * @file diskio.h
 * @brief diskio access interface
 */
#ifndef ___DISKIO_H
/*! prevent duplicate header error macro */
#define ___DISKIO_H 0

#include <types.h>

#define DISK_SLOT_PXE_INITRD_BASE  0x12000
#define DISK_SLOT_INITRD_MAX_COUNT 6

/**
 * @enum disk_slot_type
 * @brief  disk slot type
 */
typedef enum disk_slot_type {
	DISK_SLOT_TYPE_UNUSED = 0, ///< unused aka empty slot
	DISK_SLOT_TYPE_STAGE1 = 1, ///< stage1 slot aka mbr
	DISK_SLOT_TYPE_SLOTTABLE = 2, ///< slot table slot
	DISK_SLOT_TYPE_STAGE2 = 3, ///< stage2 aka real mode loader kernel
	DISK_SLOT_TYPE_STAGE3 = 4, ///< stage3 aka long mode loader kernel
	DISK_SLOT_TYPE_INITRD = 5, ///< initrd
	DISK_SLOT_TYPE_PXEINITRD = 0x85, ///< pxe initrd
}disk_slot_type_t; ///< enum short hand

/**
 * @struct disk_slot
 * @brief  disk slot definition
 */
typedef struct disk_slot {
	uint8_t type; ///< slot type look for @ref disk_slot_type
	uint8_t unused[7]; ///< unused bytes for future usage
	uint64_t start; ///< start lba address
	uint64_t end; ///< end lba address
} __attribute__ ((packed)) disk_slot_t; ///< struct short hand

/**
 * @struct disk_slot_table
 * @brief  10 entry for slots
 */
typedef struct disk_slot_table {
	disk_slot_t slots[10]; ///< slots look for @ref disk_slot_t
} __attribute__ ((packed)) disk_slot_table_t; ///< struct short hand

typedef struct {
	uint8_t head_count;
	uint8_t sector_count;
	uint16_t cylinder_count;
} disk_geometry_t;

int8_t disk_cache_geometry(uint8_t hard_disk);

/**
 * @brief read disk sectors with 48bit lba addressing
 * @param[in]  hard_disk  hard disk number
 * @param[in]  lba start address
 * @param[in]  sector_count how many sectors will be readed
 * @param[out]  data   returned data as byte array
 * @return  0 if successed.
 */
uint16_t disk_read(uint8_t hard_disk, uint64_t lba, uint16_t sector_count, uint8_t** data);

/**
 * @brief reads slot table
 * @param[in]  hard_disk  hard disk number
 * @param[out] pst  readed slot table
 * @return 0 if successed
 */
uint16_t disk_read_slottable(uint8_t hard_disk, disk_slot_table_t** pst);

#endif
