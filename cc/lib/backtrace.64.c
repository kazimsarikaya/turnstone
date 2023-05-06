/**
 * @file backtrace.64.c
 * @brief backtrace (stack trace) implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <backtrace.h>
#include <video.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>

MODULE("turnstone.lib");

stackframe_t* backtrace_get_stackframe(void) {
    stackframe_t* frame = NULL;

    asm ("mov %%rbp, %0\n" : "=r" (frame));

    program_header_t* ph = (program_header_t*)SYSTEM_INFO->kernel_start;

    if((uint64_t)frame < ph->section_locations[LINKER_SECTION_TYPE_STACK].section_start) {
        return NULL;
    }

    if((uint64_t)frame > ph->section_locations[LINKER_SECTION_TYPE_STACK].section_start + ph->section_locations[LINKER_SECTION_TYPE_STACK].section_size) {
        return NULL;
    }

    if(frame->rip < ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start) {
        return NULL;
    }

    if(frame->rip > ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start + ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_size) {
        return NULL;
    }

    return frame;
}

void backtrace_print(stackframe_t* frame) {
    PRINTLOG(KERNEL, LOG_ERROR, "Trace:");

    program_header_t* ph = (program_header_t*)SYSTEM_INFO->kernel_start;

    while(frame) {
        PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%llx RBP: 0x%p", frame->rip, frame);

        frame = frame->previous;

        if((uint64_t)frame < ph->section_locations[LINKER_SECTION_TYPE_STACK].section_start) {
            break;
        }

        if((uint64_t)frame > ph->section_locations[LINKER_SECTION_TYPE_STACK].section_start + ph->section_locations[LINKER_SECTION_TYPE_STACK].section_size) {
            break;
        }

        if(frame->rip < ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start) {
            break;
        }

        if(frame->rip > ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_start + ph->section_locations[LINKER_SECTION_TYPE_TEXT].section_size) {
            break;
        }
    }

}

void backtrace(void){
    backtrace_print(backtrace_get_stackframe());
}
