/**
 * @file systeminfo.h
 * @brief system information data for kernel entries
 */
#ifndef ___SYSTEMINFO_H
/*! prevent duplicate header error macro */
#define ___SYSTEMINFO_H 0

#include <types.h>
#include <memory.h>
#include <memory/mmap.h>

typedef enum {
	SYSTEM_INFO_BOOT_TYPE_DISK,
	SYSTEM_INFO_BOOT_TYPE_PXE
} system_info_boot_type_t;

/**
 * @struct system_info
 * @brief  system information struct
 */
typedef struct system_info {
	memory_map_t* mmap; ///< memory map, see also memory_map_t
#if ___BITS == 16  || DOXYGEN
	uint32_t mem64bitalign; ///< for realmode 64 bit alignment dummy data
#endif
	uint32_t mmap_entry_count; ///< memory map entry count
	system_info_boot_type_t boot_type; ///< boot type
}__attribute__((packed)) system_info_t; ///< struct short hand

/*! static location of system information */
extern system_info_t* SYSTEM_INFO;

#endif
