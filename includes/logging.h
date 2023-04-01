/**
 * @file logging.h
 * @brief logging headers
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___LOGGING_H
/*! prevent duplicate header error macro */
#define ___LOGGING_H 0

#include <types.h>

/**
 * @enum logging_modules_t
 * @brief logging module enums
 */
typedef enum logging_modules_t {
    KERNEL,
    MEMORY,
    SIMPLEHEAP,
    PAGING,
    FRAMEALLOCATOR,
    ACPI,
    ACPIAML,
    VIDEO,
    LINKER,
    TASKING,
    EFI,
    PCI,
    APIC,
    IOAPIC,
    TIMER,
    AHCI,
    NETWORK,
    VIRTIO,
    VIRTIONET,
    E1000,
    FAT,
    NVME,
    TOSDB,
} logging_modules_t; ///< type short hand for enum @ref logging_modules_e

/**
 * @enum logging_level_t
 * @brief loggging levels
 */
typedef enum logging_level_t {
    LOG_PANIC=0,
    LOG_FATAL=1,
    LOG_ERROR=2,
    LOG_WARNING=3,
    LOG_INFO=4,
    LOG_DEBUG=5,
    LOG_VERBOSE=7,
    LOG_TRACE=9,
} logging_level_t; ///< type short hand for enum @ref logging_level_e

/*! logging module names */
extern const char_t*const logging_module_names[];
/*! logging level names */
extern const char_t*const logging_level_names[];
/*! logging levels for each module, can be changed at run time */
extern uint8_t logging_module_levels[];

#ifndef LOG_LEVEL
/*! default log level for all modules */
#define LOG_LEVEL LOG_INFO
#endif

#ifndef LOG_LEVEL_KERNEL
/*! default log level for kernel module */
#define LOG_LEVEL_KERNEL LOG_DEBUG
#endif

#ifndef LOG_LEVEL_MEMORY
/*! default log level for memory module */
#define LOG_LEVEL_MEMORY LOG_INFO
#endif

#ifndef LOG_LEVEL_SIMPLEHEAP
/*! default log level for simple heap module */
#define LOG_LEVEL_SIMPLEHEAP LOG_INFO
#endif

#ifndef LOG_LEVEL_PAGING
/*! default log level for paging module */
#define LOG_LEVEL_PAGING LOG_INFO
#endif

#ifndef LOG_LEVEL_FRAMEALLOCATOR
/*! default log level for frame allocator module */
#define LOG_LEVEL_FRAMEALLOCATOR LOG_INFO
#endif

#ifndef LOG_LEVEL_ACPI
/*! default log level for acpi module */
#define LOG_LEVEL_ACPI LOG_INFO
#endif

#ifndef LOG_LEVEL_ACPIAML
/*! default log level for acpi aml module */
#define LOG_LEVEL_ACPIAML LOG_INFO
#endif

#ifndef LOG_LEVEL_VIDEO
/*! default log level for video module */
#define LOG_LEVEL_VIDEO LOG_INFO
#endif

#ifndef LOG_LEVEL_LINKER
/*! default log level for linker module */
#define LOG_LEVEL_LINKER LOG_TRACE
#endif

#ifndef LOG_LEVEL_TASKING
/*! default log level for tasking module */
#define LOG_LEVEL_TASKING LOG_INFO
#endif

#ifndef LOG_LEVEL_EFI
/*! default log level for efi module */
#define LOG_LEVEL_EFI LOG_INFO
#endif

#ifndef LOG_LEVEL_PCI
/*! default log level for pci module */
#define LOG_LEVEL_PCI LOG_INFO
#endif

#ifndef LOG_LEVEL_APIC
/*! default log level for apic module */
#define LOG_LEVEL_APIC LOG_INFO
#endif

#ifndef LOG_LEVEL_IOAPIC
/*! default log level for ioapic module */
#define LOG_LEVEL_IOAPIC LOG_INFO
#endif

#ifndef LOG_LEVEL_TIMER
/*! default log level for timer module */
#define LOG_LEVEL_TIMER LOG_INFO
#endif

#ifndef LOG_LEVEL_AHCI
/*! default log level for ahci module */
#define LOG_LEVEL_AHCI LOG_INFO
#endif

#ifndef LOG_LEVEL_NETWORK
/*! default log level for network module */
#define LOG_LEVEL_NETWORK LOG_INFO
#endif

#ifndef LOG_LEVEL_VIRTIO
/*! default log level for virtio module */
#define LOG_LEVEL_VIRTIO LOG_INFO
#endif

#ifndef LOG_LEVEL_VIRTIONET
/*! default log level for virtio net module */
#define LOG_LEVEL_VIRTIONET LOG_INFO
#endif

#ifndef LOG_LEVEL_E1000
/*! default log level for e1000 module */
#define LOG_LEVEL_E1000 LOG_INFO
#endif

#ifndef LOG_LEVEL_FAT
/*! default log level for fat module */
#define LOG_LEVEL_FAT LOG_INFO
#endif

#ifndef LOG_LEVEL_NVME
/*! default log level for nvme module */
#define LOG_LEVEL_NVME LOG_DEBUG
#endif

#ifndef LOG_LEVEL_TOSDB
/*! default log level for nvme module */
#define LOG_LEVEL_TOSDB LOG_DEBUG
#endif

#ifndef LOG_LOCATION
/*! file and line no will be logged? */
#define LOG_LOCATION 1
#endif

/*! checks for need logging for log message */
#define LOG_NEED_LOG(M, L)  (L <= LOG_LEVEL || L <= logging_module_levels[M])

/*! logs fellowing block */
#define LOGBLOCK(M, L) if(LOG_NEED_LOG(M, L))

#endif
