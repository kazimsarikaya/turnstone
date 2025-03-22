/**
 * @file logging.64.c
 * @brief logging implementations
 *
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <logging.h>
#include <windowmanager.h>
#include <strings.h>

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
    "IGB",
    "FAT",
    "NVME",
    "TOSDB",
    "HEAP_HASH",
    "HPET",
    "VMWARESVGA",
    "USB",
    "COMPRESSION",
    "COMPILER",
    "COMPILER_ASSEMBLER",
    "COMPILER_PASCAL",
    "HYPERVISOR",
    "HYPERVISOR_IOMMU",
    "WINDOWMANAGER",
    "PNG",
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
    LOG_LEVEL_IGB,
    LOG_LEVEL_FAT,
    LOG_LEVEL_NVME,
    LOG_LEVEL_TOSDB,
    LOG_LEVEL_HEAP_HASH,
    LOG_LEVEL_HPET,
    LOG_LEVEL_VMWARESVGA,
    LOG_LEVEL_USB,
    LOG_LEVEL_COMPRESSION,
    LOG_LEVEL_COMPILER,
    LOG_LEVEL_COMPILER_ASSEMBLER,
    LOG_LEVEL_COMPILER_PASCAL,
    LOG_LEVEL_HYPERVISOR,
    LOG_LEVEL_HYPERVISOR_IOMMU,
    LOG_LEVEL_WINDOWMANAGER,
    LOG_LEVEL_PNG,
};

boolean_t logging_need_logging(logging_modules_t module, logging_level_t level) {
    return level <= LOG_LEVEL || level <= logging_module_levels[module];
}

void logging_set_level(logging_modules_t module, logging_level_t level) {
    logging_module_levels[module] = level;
}

void logging_printlog(uint64_t module, uint64_t level, const char_t* file_name, uint64_t line_no, const char_t* format, ...) {
    if(!logging_need_logging(module, level)) {
        return;
    }

    buffer_t* buffer_error = buffer_get_io_buffer(BUFFER_IO_ERROR);
    boolean_t using_tmp_printf_buffer = false;

    if(!buffer_error) {
        buffer_error = buffer_get_tmp_buffer_for_printf();

        using_tmp_printf_buffer = true;
    }

    if(LOG_LOCATION) {
        buffer_printf(buffer_error, "%s:%lli:", file_name, line_no);
    }

    buffer_printf(buffer_error, "%s:%s:", logging_module_names[module], logging_level_names[level]);

    va_list args;
    va_start(args, format);

    buffer_vprintf(buffer_error, format, args);

    va_end(args);

    buffer_printf(buffer_error, "\n");

    if(!windowmanager_is_initialized()) {
        stdbufs_flush_buffer(buffer_error);
    }

    if(using_tmp_printf_buffer) {
        buffer_reset_tmp_buffer_for_printf();
    }
}

int8_t logging_set_level_by_string_values(const char_t* module, const char_t* level) {
    char_t* mnu = struppercopy(module);
    char_t* lvl = struppercopy(level);

    int32_t module_index = -1;
    int32_t level_index = -1;

    for(uint32_t i = 0; i < sizeof(logging_module_names) / sizeof(logging_module_names[0]); i++) {
        if(strcmp(mnu, logging_module_names[i]) == 0) {
            module_index = i;
            break;
        }
    }

    for(uint32_t i = 0; i < sizeof(logging_level_names) / sizeof(logging_level_names[0]); i++) {
        if(strcmp(lvl, logging_level_names[i]) == 0) {
            level_index = i;
            break;
        }
    }

    if(module_index == -1 || level_index == -1) {
        memory_free(mnu);
        memory_free(lvl);
        return -1;
    }

    logging_module_levels[module_index] = level_index;

    memory_free(mnu);
    memory_free(lvl);

    return 0;
}
