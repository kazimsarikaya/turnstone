/**
 * @file logging.64.c
 * @brief logging implementations
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <logging.h>

MODULE("turnstone.lib.logging");


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
    "COMPILER",
    "COMPILER_ASSEMBLER",
    "COMPILER_PASCAL",
    "HYPERVISOR",
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

logging_level_t logging_module_levels[] = {
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
    LOG_LEVEL_COMPILER,
    LOG_LEVEL_COMPILER_ASSEMBLER,
    LOG_LEVEL_COMPILER_PASCAL,
    LOG_LEVEL_HYPERVISOR,
};

extern boolean_t windowmanager_initialized;

void logging_printlog(uint64_t module, uint64_t level, const char_t* file_name, uint64_t line_no, const char_t* format, ...) {
    if(!LOG_NEED_LOG(module, level)) {
        return;
    }

    buffer_t* buffer_error = buffer_get_io_buffer(BUFFER_IO_ERROR);
    boolean_t using_tmp_printf_buffer = false;

    if(!buffer_error) {
        buffer_error = buffer_get_tmp_buffer_for_printf();

        using_tmp_printf_buffer = true;
    }

    uint64_t buffer_old_position = buffer_get_length(buffer_error);

    if(LOG_LOCATION) {
        buffer_printf(buffer_error, "%s:%lli:", file_name, line_no);
    }

    buffer_printf(buffer_error, "%s:%s:", logging_module_names[module], logging_level_names[level]);

    va_list args;
    va_start(args, format);

    buffer_vprintf(buffer_error, format, args);

    va_end(args);

    buffer_printf(buffer_error, "\n");

    if(!windowmanager_initialized) {
        stdbufs_flush_buffer(buffer_error, buffer_old_position);
    }

    if(using_tmp_printf_buffer) {
        buffer_reset_tmp_buffer_for_printf();
    }
}
