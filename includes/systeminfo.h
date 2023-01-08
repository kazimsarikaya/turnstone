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

/**
 * @enum system_info_boot_type_e
 * @brief system boot type
 */
typedef enum system_info_boot_type_e {
    SYSTEM_INFO_BOOT_TYPE_DISK, ///< system booted from disk
    SYSTEM_INFO_BOOT_TYPE_PXE, ///< system booted from network (pxe)
} system_info_boot_type_t; ///< short hand for enum system_info_boot_type_e

/**
 * @struct system_info_s
 * @brief  system information struct
 */
typedef struct system_info_s {
    memory_heap_t*        heap; ///< kernel heap
    uint8_t*              mmap_data; ///< uefi mmap data
    uint64_t              mmap_size; ///< uefi mmap size
    uint64_t              mmap_descriptor_size; ///< uefi mmap descriptor size
    uint64_t              mmap_descriptor_version; ///<uefi mmap descriptor version
    uint64_t              boot_type; ///< boot type @sa system_info_boot_type_t
    video_frame_buffer_t* frame_buffer; ///< video frame buffer address, delivered from uefi
    uint64_t              acpi_version; ///< acpi table version
    void*                 acpi_table; ///< acpi table address
    uint64_t              kernel_start; ///< kernel start address
    uint64_t              kernel_4k_frame_count; ///< kernel frame count in 4k bytes
    uint64_t              remapped; ///< is kernel remapped?
    uint64_t              my_page_table; ///< kernel's page table, for uefi's pt diff
    uint8_t*              reserved_mmap_data; ///< reserved mmap data for remapping
    uint64_t              reserved_mmap_size; ///< size of reserved mmap data for remapping
    efi_system_table_t*   efi_system_table; ///< accessing efi tables from kernel
} system_info_t; ///< struct short hand for system_info_s

/*! static location of system information */
extern system_info_t* SYSTEM_INFO;

#endif
