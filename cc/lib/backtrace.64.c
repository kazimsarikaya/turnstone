/**
 * @file backtrace.64.c
 * @brief backtrace (stack trace) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <backtrace.h>
#include <logging.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <cpu.h>
#include <cpu/task.h>
#include <bplustree.h>

MODULE("turnstone.lib");

static index_t* backtrace_symbol_table = NULL;

static int8_t backtrace_symbol_table_compare_function(const void* a, const void* b) {
    linker_global_offset_table_entry_t* entry_a = (linker_global_offset_table_entry_t*)a;
    linker_global_offset_table_entry_t* entry_b = (linker_global_offset_table_entry_t*)b;

    uint64_t entry_a_start = entry_a->entry_value;
    uint64_t entry_a_end = entry_a->entry_value + entry_a->symbol_size - 1;

    uint64_t entry_b_start = entry_b->entry_value;
    uint64_t entry_b_end = entry_b->entry_value + entry_b->symbol_size - 1;

    if(entry_a_end < entry_b_start) {
        return -1;
    }

    if(entry_a_start > entry_b_end) {
        return 1;
    }

    return 0;
}

static const char_t* backtrace_get_symbol_name_by_symbol_name_offset(uint64_t symbol_name_offset) {
    program_header_t* ph = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    const char_t* symbol_name = (const char_t*)(ph->symbol_table_virtual_address + symbol_name_offset);

    return symbol_name;
}

static int8_t backtrace_got_key_cloner(memory_heap_t* heap, const void* key, void** cloned_key) {
    *cloned_key = memory_malloc_ext(heap, sizeof(linker_global_offset_table_entry_t), 0);

    if(!*cloned_key) {
        return -1;
    }

    memory_memcopy(key, *cloned_key, sizeof(linker_global_offset_table_entry_t));

    return 0;
}

int8_t backtrace_init(void) {
    PRINTLOG(KERNEL, LOG_INFO, "Initializing backtrace");

    backtrace_symbol_table = bplustree_create_index_with_unique(128, backtrace_symbol_table_compare_function, true);

    if(!backtrace_symbol_table) {
        return -1;
    }

    bplustree_set_key_cloner(backtrace_symbol_table, backtrace_got_key_cloner);

    program_header_t* ph = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    linker_global_offset_table_entry_t* got = (linker_global_offset_table_entry_t*)ph->got_virtual_address;

    uint64_t got_entry_count = ph->got_size / sizeof(linker_global_offset_table_entry_t);

    for(uint64_t i = 0; i < got_entry_count; i++) {
        if(got[i].symbol_type == LINKER_SYMBOL_TYPE_FUNCTION) {
            // const char_t* symbol_name = backtrace_get_symbol_name_by_symbol_name_offset(got[i].symbol_name_offset);
            // PRINTLOG(KERNEL, LOG_INFO, "Adding symbol: 0x%llx 0x%llx 0x%llx 0x%llx %s",
            // got[i].entry_value, got[i].symbol_value, got[i].symbol_size, got[i].symbol_name_offset, symbol_name);
            if(backtrace_symbol_table->insert(backtrace_symbol_table, &got[i], &got[i], NULL) != 0) {
                return -1;
            }
        }
    }

/*
    iterator_t* iter = backtrace_symbol_table->create_iterator(backtrace_symbol_table);

    if(!iter) {
        return -1;
    }

    while(iter->end_of_iterator(iter) != 0) {
        const linker_global_offset_table_entry_t* got_entry = iter->get_item(iter);

        const char_t* symbol_name = backtrace_get_symbol_name_by_symbol_name_offset(got_entry->symbol_name_offset);

        PRINTLOG(KERNEL, LOG_INFO, "Symbol: 0x%llx 0x%llx 0x%llx 0x%llx %s",
                 got_entry->entry_value, got_entry->symbol_value, got_entry->symbol_size, got_entry->symbol_name_offset, symbol_name);

        iter->next(iter);
    }

    iter->destroy(iter);
 */

    return 0;
}

static const linker_global_offset_table_entry_t* backtrace_get_symbol_entry(uint64_t rip) {
    if(!backtrace_symbol_table) {
        return NULL;
    }

    linker_global_offset_table_entry_t entry = {0};
    entry.entry_value = rip;
    entry.symbol_size = 1;

    const linker_global_offset_table_entry_t * got_entry = backtrace_symbol_table->find(backtrace_symbol_table, &entry);

    return got_entry;
}

const char_t* backtrace_get_symbol_name_by_rip(uint64_t rip) {
    const linker_global_offset_table_entry_t* got_entry = backtrace_get_symbol_entry(rip);

    if(!got_entry) {
        return NULL;
    }

    const char_t* symbol_name = backtrace_get_symbol_name_by_symbol_name_offset(got_entry->symbol_name_offset);

    return symbol_name;
}

static inline stackframe_t* backtrace_validate_stackframe(stackframe_t* frame) {
    task_t* task = task_get_current_task();

    program_header_t* ph = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    uint64_t stack_start = 0;
    uint64_t stack_end = 0;
    uint64_t stack_size = 0;

    if(task) {
        PRINTLOG(KERNEL, LOG_ERROR, "Task Id: %llx", task->task_id);
        stack_end = (uint64_t)task->stack;
        stack_start = stack_end - task->stack_size;
        stack_size = task->stack_size;
    } else {
        stack_start = ph->program_stack_virtual_address;
        stack_end = stack_start + ph->program_stack_size;
        stack_size = ph->program_stack_size;
    }

    PRINTLOG(KERNEL, LOG_ERROR, "Stack start: 0x%llx end: 0x%llx size: 0x%llx", stack_start, stack_end, stack_size);

    if(stack_start == 0 || stack_end == 0) {
        return NULL;
    }

    if((uint64_t)frame < stack_start) {
        return NULL;
    }

    if((uint64_t)frame > stack_end) {
        return NULL;
    }

    /*
       if(frame->rip < ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start) {
        return NULL;
       }

       if(frame->rip > ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start + ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_size) {
        return NULL;
       }
     */

    return frame;
}

stackframe_t* backtrace_get_stackframe(void) {
    stackframe_t* frame = NULL;

    asm ("mov %%rbp, %0\n" : "=r" (frame));

    frame += 2;

    return backtrace_validate_stackframe(frame);
}

void backtrace_print(stackframe_t* frame) {
    PRINTLOG(KERNEL, LOG_ERROR, "Trace by frame 0x%p:", frame);

    if(!frame) {
        return;
    }

    uint8_t max_frames = 20;

    while(max_frames--) {
        linker_global_offset_table_entry_t* got_entry = (linker_global_offset_table_entry_t*)backtrace_get_symbol_entry(frame->rip);

        if(got_entry) {
            const char_t* symbol_name = backtrace_get_symbol_name_by_symbol_name_offset(got_entry->symbol_name_offset);

            PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%llx RBP: 0x%p %s", frame->rip, frame, symbol_name);
        } else {
            PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%llx RBP: 0x%p", frame->rip, frame);
        }

        frame = frame->previous;

        if(!frame) {
            break;
        }

        // frame = backtrace_validate_stackframe(frame);
    }

    // cpu_hlt();
}

void backtrace(void){
    backtrace_print(backtrace_get_stackframe());
}

stackframe_t* backtrace_print_interrupt_registers(uint64_t rsp) {
    uint64_t* registers = (uint64_t*)(rsp + 0x18);

    PRINTLOG(KERNEL, LOG_ERROR, "Registers:");
    PRINTLOG(KERNEL, LOG_ERROR, "\tRAX: 0x%llx", registers[0]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRDX: 0x%llx", registers[1]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRCX: 0x%llx", registers[2]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRBX: 0x%llx", registers[3]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRSI: 0x%llx", registers[4]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRDI: 0x%llx", registers[5]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR8: 0x%llx", registers[6]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR9: 0x%llx", registers[7]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR10: 0x%llx", registers[8]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR11: 0x%llx", registers[9]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR12: 0x%llx", registers[10]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR13: 0x%llx", registers[11]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR14: 0x%llx", registers[12]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR15: 0x%llx", registers[13]);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRBP: 0x%llx", registers[14]);

    return (stackframe_t*)registers[14];
}
