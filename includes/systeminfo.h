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
#include <driver/video_fb.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum system_info_boot_type_t
 * @brief system boot type
 */
typedef enum system_info_boot_type_t {
    SYSTEM_INFO_BOOT_TYPE_DISK, ///< system booted from disk
    SYSTEM_INFO_BOOT_TYPE_PXE, ///< system booted from network (pxe)
} system_info_boot_type_t; ///< short hand for enum system_info_boot_type_e

/**
 * @struct system_info_t
 * @brief  system information struct
 */
typedef struct system_info_t {
    uint8_t*              mmap_data; ///< uefi mmap data
    uint64_t              mmap_size; ///< uefi mmap size
    uint64_t              mmap_descriptor_size; ///< uefi mmap descriptor size
    uint64_t              mmap_descriptor_version; ///<uefi mmap descriptor version
    uint64_t              boot_type; ///< boot type @sa system_info_boot_type_t
    video_frame_buffer_t* frame_buffer; ///< video frame buffer address, delivered from uefi
    uint64_t              acpi_version; ///< acpi table version
    void*                 acpi_rsdp; ///< acpi rsdp address
    void*                 acpi_xrsdp; ///< acpi xrsdp address
    uint64_t              program_header_virtual_start; ///< program virtual start address
    uint64_t              program_header_physical_start; ///< program physical start address
    efi_system_table_t*   efi_system_table; ///< accessing efi tables from kernel
    void*                 smbios_table_v2; ///< smbios table v2 address
    void*                 smbios_table_v3; ///< smbios table v3 address
    uint64_t              pxe_tosdb_size; ///< pxe tosdb size
    uint64_t              pxe_tosdb_address; ///< pxe tosdb address
    uint64_t              random_seed; ///< random seed
    uint64_t              spool_size; ///< spool size
    uint64_t              spool_physical_start; ///< spool physical start
    uint64_t              spool_virtual_start; ///< spool virtual start
    uint64_t              interrupt_handlers_module_id; ///< the module id of interrupt handlers, used for task switching for userspace processes
    uint64_t              gs_page_address_base;
    uint64_t              gs_page_size;
    uint64_t              cpu_count; ///< cpu count
    uint64_t              proximity_domain_count; ///< proximity domain count
    uint32_t*             cpu_proximity_domain_array; ///< cpu proximity domain array, indexed by local apic id, with size of cpu count
} system_info_t; ///< struct short hand for system_info_t

_Static_assert(sizeof(system_info_t) == 208, "system_info_t size should be 208 bytes");

/*! static location of system information */
extern system_info_t* SYSTEM_INFO;

#ifdef __cplusplus
}
#endif

#endif
