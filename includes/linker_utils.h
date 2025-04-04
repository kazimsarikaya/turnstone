/**
 * @file linker_utils.h
 * @brief defines linker related structures.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___LINKER_UTILS_H
#define ___LINKER_UTILS_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct linker_module_at_memory_t {
    uint64_t id;
    uint64_t module_name_offset;
    uint64_t physical_start;
    uint64_t virtual_start;
} linker_module_at_memory_t;

typedef struct linker_section_at_memory_t {
    uint64_t section_type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t size;
} linker_section_at_memory_t;

typedef union linker_metadata_at_memory_t {
    linker_module_at_memory_t  module;
    linker_section_at_memory_t section;
} linker_metadata_at_memory_t;

#ifdef __cplusplus
}
#endif

#endif
