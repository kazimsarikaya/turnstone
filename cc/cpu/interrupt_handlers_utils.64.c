/**
 * @file interrupt_handlers_utils.64.c
 * @brief Utility functions for interrupt handlers.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <cpu/interrupt.h>
#include <memory/paging.h>
#include <linker.h>
#include <linker_utils.h>
#include <systeminfo.h>
#include <logging.h>

MODULE("turnstone.kernel.cpu.interrupt.handlers.utils");

int8_t interrupt_handlers_make_readonly(void) {
    const linker_metadata_at_memory_t* module_or_section = linker_get_module_at_memory(SYSTEM_INFO->interrupt_handlers_module_id);

    if(!module_or_section) {
        PRINTLOG(KERNEL, LOG_ERROR, "cannot find interrupt handlers module in memory");
        return -1;
    }

    const linker_section_at_memory_t* bss_section = NULL;

    module_or_section++;
    while(module_or_section->section.size) {
        module_or_section++;

        if(module_or_section->section.section_type == LINKER_SECTION_TYPE_BSS) {
            bss_section = &module_or_section->section;
            break;
        }
    }

    if(!bss_section) {
        PRINTLOG(KERNEL, LOG_ERROR, "cannot find bss section for interrupt handlers module");
        return -1;
    }

    if(memory_paging_toggle_attributes(bss_section->virtual_start, MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
        PRINTLOG(KERNEL, LOG_ERROR, "cannot toggle readonly attribute for interrupt handlers bss section");
    }

    return 0;
}
