/**
 * @file systeminfo.h
 * @brief system information data for kernel entries
 */
#ifndef ___SYSTEMINFO_H
/*! prevent duplicate header error macro */
#define ___SYSTEMINFO_H 0

#include <types.h>
#include <memory.h>

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
}__attribute__((packed)) system_info_t; ///< struct short hand

/*! static location of system information */
extern system_info_t* SYSTEM_INFO;

#endif
