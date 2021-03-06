/**
 * @file systeminfo.h
 * @brief system information data for kernel entries
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___SYSTEMINFO_H
/*! prevent duplicate header error macro */
#define ___SYSTEMINFO_H 0

#include <types.h>
#include <memory.h>
#include <efi.h>
#include <video.h>

typedef enum {
	SYSTEM_INFO_BOOT_TYPE_DISK,
	SYSTEM_INFO_BOOT_TYPE_PXE
} system_info_boot_type_t;

/**
 * @struct system_info
 * @brief  system information struct
 */
typedef struct system_info {
	memory_heap_t* heap;
	uint8_t* mmap_data;
	uint64_t mmap_size;
	uint64_t mmap_descriptor_size;
	uint32_t mmap_descriptor_version;
	system_info_boot_type_t boot_type;
	video_frame_buffer_t* frame_buffer;
	uint64_t acpi_version;
	void* acpi_table;
	uint64_t kernel_start;
	uint64_t kernel_4k_frame_count;
	boolean_t remapped;
	boolean_t my_page_table;
	uint8_t* reserved_mmap_data;
	uint64_t reserved_mmap_size;
	efi_system_table_t* efi_system_table;
} system_info_t; ///< struct short hand

/*! static location of system information */
extern system_info_t* SYSTEM_INFO;

#endif
