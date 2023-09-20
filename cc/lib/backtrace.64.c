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

MODULE("turnstone.lib");

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

    uint8_t max_frames = 10;

    while(max_frames--) {
        PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%llx RBP: 0x%p", frame->rip, frame);

        frame = frame->previous;

        if(!frame) {
            break;
        }

        //frame = backtrace_validate_stackframe(frame);
    }

    //cpu_hlt();
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
