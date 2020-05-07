/**
 * @file mmap.h
 * @brief memory map detection interface
 */
#ifndef ___MEMORY_MMAP_H
/*! prevent duplicate header error macro */
#define ___MEMORY_MMAP_H 0

#include <memory.h>

/*! while memory detection max memory entry count to search */
#define MEMORY_MMAP_MAX_ENTRY_COUNT 128

/**
 * @enum memory_map_type_t
 * @brief memory type enum
 */
typedef enum {
	MEMORY_MMAP_TYPE_USABLE = 1, ///< usable memory
	MEMORY_MMAP_TYPE_RESERVED = 2, ///< reserved memory for cpu usages
	MEMORY_MMAP_TYPE_ACPI = 3, ///< acpi memory
	MEMORY_MMAP_TYPE_ACPI_NVS = 4 ///< acpi nvs memory
}memory_map_type_t; ///< short hand for enum

/**
 * @struct memory_map_t
 * @brief memory map struct
 *
 * memory map is learned by bios interrupt. map is consist of memory holes.
 */
typedef struct {
	uint64_t base; ///< base address of memory
	uint64_t length; ///<memory length
	uint32_t type; ///< memory map type, see also \ref memory_map_type_t
	uint32_t acpi; ///< acpi extension never seen
} __attribute__((packed)) memory_map_t; ///< short hand for struct

/**
 * @brief detect memory map
 * @param[out]  map detected memory map
 * @return      entry count
 *
 * detects memory map with bios interrupt
 */
size_t memory_detect_map(memory_map_t** map);

#endif
