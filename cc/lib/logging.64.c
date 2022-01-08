#include <logging.h>


char_t* logging_module_names[] = {
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
};


char_t* logging_level_names[] = {
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
};
