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
#include <buffer.h>
#include <stdbufs.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    IGB,
    FAT,
    NVME,
    TOSDB,
    HEAP_HASH,
    HPET,
    VMWARESVGA,
    USB,
    COMPRESSION,
    COMPILER,
    COMPILER_ASSEMBLER,
    COMPILER_PASCAL,
    HYPERVISOR,
    WINDOWMANAGER,
    PNG,
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

#if 0
/*! logging module names */
extern const char_t*const logging_module_names[];
/*! logging level names */
extern const char_t*const logging_level_names[];
/*! logging levels for each module, can be changed at run time */
extern logging_level_t logging_module_levels[];
#endif

#ifndef LOG_LEVEL
/*! default log level for all modules */
#define LOG_LEVEL LOG_INFO
#endif

#ifndef LOG_LEVEL_KERNEL
/*! default log level for kernel module */
#define LOG_LEVEL_KERNEL LOG_INFO
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
#define LOG_LEVEL_LINKER LOG_INFO
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

#ifndef LOG_LEVEL_IGB
/*! default log level for e1000 module */
#define LOG_LEVEL_IGB LOG_INFO
#endif

#ifndef LOG_LEVEL_FAT
/*! default log level for fat module */
#define LOG_LEVEL_FAT LOG_INFO
#endif

#ifndef LOG_LEVEL_NVME
/*! default log level for nvme module */
#define LOG_LEVEL_NVME LOG_INFO
#endif

#ifndef LOG_LEVEL_TOSDB
/*! default log level for nvme module */
#define LOG_LEVEL_TOSDB LOG_INFO
#endif

#ifndef LOG_LEVEL_HEAP_HASH
/*! default log level for heap hash module */
#define LOG_LEVEL_HEAP_HASH LOG_INFO
#endif

#ifndef LOG_LEVEL_HPET
/*! default log level for hpet module */
#define LOG_LEVEL_HPET LOG_INFO
#endif

#ifndef LOG_LEVEL_VMWARESVGA
/*! default log level for vmware svga module */
#define LOG_LEVEL_VMWARESVGA LOG_INFO
#endif

#ifndef LOG_LEVEL_USB
/*! default log level for usb module */
#define LOG_LEVEL_USB LOG_INFO
#endif

#ifndef LOG_LEVEL_COMPRESSION
/*! default log level for compression module */
#define LOG_LEVEL_COMPRESSION LOG_INFO
#endif

#ifndef LOG_LEVEL_COMPILER
/*! default log level for compiler module */
#define LOG_LEVEL_COMPILER LOG_INFO
#endif

#ifndef LOG_LEVEL_COMPILER_ASSEMBLER
/*! default log level for compiler assembler module */
#define LOG_LEVEL_COMPILER_ASSEMBLER LOG_INFO
#endif

#ifndef LOG_LEVEL_COMPILER_PASCAL
/*! default log level for compiler pascal module */
#define LOG_LEVEL_COMPILER_PASCAL LOG_INFO
#endif

#ifndef LOG_LEVEL_HYPERVISOR
/*! default log level for hypervisor module */
#define LOG_LEVEL_HYPERVISOR LOG_INFO
#endif

#ifndef LOG_LEVEL_WINDOWMANAGER
/*! default log level for window manager module */
#define LOG_LEVEL_WINDOWMANAGER LOG_INFO
#endif

#ifndef LOG_LEVEL_PNG
/*! default log level for png module */
#define LOG_LEVEL_PNG LOG_INFO
#endif

#ifndef LOG_LOCATION
/*! file and line no will be logged? */
#define LOG_LOCATION 1
#endif

boolean_t logging_need_logging(logging_modules_t module, logging_level_t level);

void logging_set_level(logging_modules_t module, logging_level_t level);

/*! checks for need logging for log message */
#define LOG_NEED_LOG(M, L) logging_need_logging(M, L)

/*! logs fellowing block */
#define LOGBLOCK(M, L) if(LOG_NEED_LOG(M, L))

/**
 * @brief kernel logging function
 * @param[in] module module name @sa logging_modules_e
 * @param[in] level log level @sa logging_level_e
 * @param[in] line_no line number
 * @param[in] file_name file name
 * @param[in] format format string
 * @param[in] ... arguments for format in msg
 */
void logging_printlog(uint64_t module, uint64_t level, const char_t* file_name, uint64_t line_no, const char_t* format, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief kernel logging macro
 * @param[in] M module name @sa logging_modules_e
 * @param[in] L log level @sa logging_level_e
 * @param[in] msg log message, also if it contains after this there should be a variable arg list
 * @param[in] ... arguments for format in msg
 */
#define PRINTLOG(M, L, msg, ...)  logging_printlog(M, L, __FILE__, __LINE__, msg, ## __VA_ARGS__)


#define NOTIMPLEMENTEDLOG(M) PRINTLOG(M, LOG_ERROR, "not implemented: %s", __FUNCTION__)

int8_t logging_set_level_by_string_values(const char_t* module, const char_t* level);

#ifdef __cplusplus
}
#endif

#endif
