/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___LOGGING_H
#define ___LOGGING_H 0

#include <types.h>

typedef enum {
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
} logging_modules_t;

typedef enum {
    LOG_PANIC=0,
    LOG_FATAL=1,
    LOG_ERROR=2,
    LOG_WARNING=3,
    LOG_INFO=4,
    LOG_DEBUG=5,
    LOG_VERBOSE=7,
    LOG_TRACE=9,
} logging_level_t;

extern const char_t* logging_module_names[];
extern const char_t* logging_level_names[];
extern uint8_t logging_module_levels[];

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#endif

#ifndef LOG_LEVEL_KERNEL
#define LOG_LEVEL_KERNEL LOG_INFO
#endif

#ifndef LOG_LEVEL_MEMORY
#define LOG_LEVEL_MEMORY LOG_INFO
#endif

#ifndef LOG_LEVEL_SIMPLEHEAP
#define LOG_LEVEL_SIMPLEHEAP LOG_INFO
#endif

#ifndef LOG_LEVEL_PAGING
#define LOG_LEVEL_PAGING LOG_INFO
#endif

#ifndef LOG_LEVEL_FRAMEALLOCATOR
#define LOG_LEVEL_FRAMEALLOCATOR LOG_INFO
#endif

#ifndef LOG_LEVEL_ACPI
#define LOG_LEVEL_ACPI LOG_INFO
#endif

#ifndef LOG_LEVEL_ACPIAML
#define LOG_LEVEL_ACPIAML LOG_INFO
#endif

#ifndef LOG_LEVEL_VIDEO
#define LOG_LEVEL_VIDEO LOG_INFO
#endif

#ifndef LOG_LEVEL_LINKER
#define LOG_LEVEL_LINKER LOG_INFO
#endif

#ifndef LOG_LEVEL_TASKING
#define LOG_LEVEL_TASKING LOG_INFO
#endif

#ifndef LOG_LEVEL_EFI
#define LOG_LEVEL_EFI LOG_INFO
#endif

#ifndef LOG_LEVEL_PCI
#define LOG_LEVEL_PCI LOG_INFO
#endif

#ifndef LOG_LEVEL_APIC
#define LOG_LEVEL_APIC LOG_INFO
#endif

#ifndef LOG_LEVEL_IOAPIC
#define LOG_LEVEL_IOAPIC LOG_INFO
#endif

#ifndef LOG_LEVEL_TIMER
#define LOG_LEVEL_TIMER LOG_INFO
#endif

#ifndef LOG_LEVEL_AHCI
#define LOG_LEVEL_AHCI LOG_INFO
#endif

#ifndef LOG_LEVEL_NETWORK
#define LOG_LEVEL_NETWORK LOG_INFO
#endif

#ifndef LOG_LEVEL_VIRTIO
#define LOG_LEVEL_VIRTIO LOG_INFO
#endif

#ifndef LOG_LEVEL_VIRTIONET
#define LOG_LEVEL_VIRTIONET LOG_INFO
#endif

#ifndef LOG_LEVEL_E1000
#define LOG_LEVEL_E1000 LOG_INFO
#endif

#ifndef LOG_LEVEL_FAT
#define LOG_LEVEL_FAT LOG_INFO
#endif


#ifndef LOG_LOCATION
#define LOG_LOCATION 1
#endif

#define LOG_NEED_LOG(M, L)  (L <= LOG_LEVEL || L <= logging_module_levels[M])

#define LOGBLOCK(M, L) if(LOG_NEED_LOG(M, L))

#endif
