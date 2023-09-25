/**
 * @file logging.64.c
 * @brief logging implementations
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <logging.h>

MODULE("turnstone.lib");


const char_t*const logging_module_names[] = {
    "KERNEL",
    "MEMORY",
    "SIMPLEHEAP",
    "PAGING",
    "FRAMEALLOCATOR",
    "ACPI",
    "ACPIAML",
    "VIDEO",
    "LINKER",
    "TASKING",
    "EFI",
    "PCI",
    "APIC",
    "IOAPIC",
    "TIMER",
    "AHCI",
    "NETWORK",
    "VIRTIO",
    "VIRTIONET",
    "E1000",
    "FAT",
    "NVME",
    "TOSDB",
    "HEAP_HASH",
    "VIRTIOGPU",
    "HPET",
    "VMWARESVGA",
    "USB",
    "COMPRESSION",
};


const char_t*const logging_level_names[] = {
    "PANIC" /** 0 **/,
    "FATAL" /** 1 **/,
    "ERROR" /** 2 **/,
    "WARNING" /** 3 **/,
    "INFO" /** 4 **/,
    "DEBUG" /** 5 **/,
    "LEVEL6" /** 6 **/,
    "VERBOSE" /** 7 **/,
    "LEVEL8" /** 8 **/,
    "TRACE" /** 9 **/,
};

uint8_t logging_module_levels[] = {
    LOG_LEVEL_KERNEL,
    LOG_LEVEL_MEMORY,
    LOG_LEVEL_SIMPLEHEAP,
    LOG_LEVEL_PAGING,
    LOG_LEVEL_FRAMEALLOCATOR,
    LOG_LEVEL_ACPI,
    LOG_LEVEL_ACPIAML,
    LOG_LEVEL_VIDEO,
    LOG_LEVEL_LINKER,
    LOG_LEVEL_TASKING,
    LOG_LEVEL_EFI,
    LOG_LEVEL_PCI,
    LOG_LEVEL_APIC,
    LOG_LEVEL_IOAPIC,
    LOG_LEVEL_TIMER,
    LOG_LEVEL_AHCI,
    LOG_LEVEL_NETWORK,
    LOG_LEVEL_VIRTIO,
    LOG_LEVEL_VIRTIONET,
    LOG_LEVEL_E1000,
    LOG_LEVEL_FAT,
    LOG_LEVEL_NVME,
    LOG_LEVEL_TOSDB,
    LOG_LEVEL_HEAP_HASH,
    LOG_LEVEL_VIRTIOGPU,
    LOG_LEVEL_HPET,
    LOG_LEVEL_VMWARESVGA,
    LOG_LEVEL_USB,
    LOG_LEVEL_COMPRESSION,
};
