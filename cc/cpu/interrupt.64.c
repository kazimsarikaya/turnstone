/**
 * @file interrupt.64.c
 * @brief Interrupts.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <cpu/descriptor.h>
#include <cpu/crx.h>
#include <logging.h>
#include <strings.h>
#include <memory/paging.h>
#include <memory.h>
#include <apic.h>
#include <cpu/task.h>
#include <backtrace.h>
#include <debug.h>

MODULE("turnstone.kernel.cpu.interrupt");

void video_text_print(char_t* string);

void interrupt_register_dummy_handlers(descriptor_idt_t*);

typedef struct interrupt_irq_list_item_t {
    interrupt_irq                     irq;
    struct interrupt_irq_list_item_t* next;
} interrupt_irq_list_item_t;

interrupt_irq_list_item_t** interrupt_irqs = NULL;
uint8_t next_empty_interrupt = 0;

void interrupt_generic_handler(interrupt_frame_ext_t* frame);

int8_t interrupt_int01_debug_exception(interrupt_frame_ext_t*);
int8_t interrupt_int02_nmi_interrupt(interrupt_frame_ext_t*);
int8_t interrupt_int03_breakpoint_exception(interrupt_frame_ext_t*);
int8_t interrupt_int0D_general_protection_exception(interrupt_frame_ext_t*);
int8_t interrupt_int0E_page_fault_exception(interrupt_frame_ext_t*);
int8_t interrupt_int13_simd_floating_point_exception(interrupt_frame_ext_t*);

boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t interrupt_init(void) {
    descriptor_idt_t* idt_table = (descriptor_idt_t*)IDT_REGISTER->base;

    interrupt_register_dummy_handlers(idt_table); // 32-255 dummy handlers

    interrupt_irqs = memory_malloc(sizeof(interrupt_irq_list_item_t*) * (256));

    if(interrupt_irqs == NULL) {
        return -1;
    }

    memory_memclean(interrupt_irqs, sizeof(interrupt_irq_list_item_t*) * (256));

    interrupt_irqs[0x01] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x01] == NULL) {
        return -1;
    }

    interrupt_irqs[0x01]->irq = interrupt_int01_debug_exception;

    interrupt_irqs[0x02] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x02] == NULL) {
        return -1;
    }

    interrupt_irqs[0x02]->irq = interrupt_int02_nmi_interrupt;

    interrupt_irqs[0x03] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x03] == NULL) {
        return -1;
    }

    interrupt_irqs[0x03]->irq = interrupt_int03_breakpoint_exception;

    interrupt_irqs[0x0D] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x0D] == NULL) {
        return -1;
    }

    interrupt_irqs[0x0D]->irq = interrupt_int0D_general_protection_exception;

    interrupt_irqs[0x0E] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x0E] == NULL) {
        return -1;
    }

    interrupt_irqs[0x0E]->irq = interrupt_int0E_page_fault_exception;

    interrupt_irqs[0x13] = memory_malloc(sizeof(interrupt_irq_list_item_t));

    if(interrupt_irqs[0x13] == NULL) {
        return -1;
    }

    interrupt_irqs[0x13]->irq = interrupt_int13_simd_floating_point_exception;

    next_empty_interrupt = INTERRUPT_IRQ_BASE;

    cpu_sti();

    return 0;
}
#pragma GCC diagnostic pop

int8_t interrupt_redirect_main_interrupts(uint8_t ist) {
    cpu_cli();
    descriptor_idt_t* idt_table = (descriptor_idt_t*)IDT_REGISTER->base;

    for(int32_t i = 0; i < 32; i++) {
        idt_table[i].ist = ist;
    }

    cpu_sti();

    return 0;
}

uint8_t interrupt_get_next_empty_interrupt(void){
    uint8_t res = next_empty_interrupt;
    next_empty_interrupt++;
    return res;
}

int8_t interrupt_irq_remove_handler(uint8_t irqnum, interrupt_irq irq) {
    if(interrupt_irqs == NULL) {
        return -1;
    }

    cpu_cli();

    irqnum += 32;

    if(interrupt_irqs[irqnum]) {
        interrupt_irq_list_item_t* item = interrupt_irqs[irqnum];
        interrupt_irq_list_item_t* prev = NULL;

        while(item && item->irq != irq) {
            prev = item;
            item = item->next;
        }

        if(item) {
            if(prev) {
                prev->next = item->next;
            } else {
                interrupt_irqs[irqnum] = NULL;
            }

            memory_free(item);
        }

    } else {
        return -1;
    }

    cpu_sti();

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t interrupt_irq_set_handler(uint8_t irqnum, interrupt_irq irq) {
    if(interrupt_irqs == NULL) {
        return -1;
    }

    const char_t* return_symbol_name = backtrace_get_symbol_name_by_rip((uint64_t)irq);

    PRINTLOG(KERNEL, LOG_INFO, "Setting IRQ handler for IRQ 0x%x func at 0x%p %s", irqnum, irq, return_symbol_name);

    cpu_cli();

    irqnum += 32;

    if(interrupt_irqs[irqnum] == NULL) {
        interrupt_irqs[irqnum] = memory_malloc(sizeof(interrupt_irq_list_item_t));

        if(interrupt_irqs[irqnum] == NULL) {
            return -1;
        }

        interrupt_irqs[irqnum]->irq = irq;

    } else {
        interrupt_irq_list_item_t* item = interrupt_irqs[irqnum];

        while(item->next) {
            item = item->next;
        }

        item->next = memory_malloc(sizeof(interrupt_irq_list_item_t));

        if(item->next == NULL) {
            return -1;
        }

        item->next->irq = irq;
    }

    cpu_sti();

    PRINTLOG(KERNEL, LOG_TRACE, "IRQ handler set for IRQ 0x%x func at 0x%p", irqnum, irq);

    return 0;
}
#pragma GCC diagnostic pop

static void interrupt_print_frame_ext(interrupt_frame_ext_t* frame) {
    PRINTLOG(KERNEL, LOG_ERROR, "Interrupt frame:");
    PRINTLOG(KERNEL, LOG_ERROR, "\tRAX: 0x%llx", frame->rax);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRBX: 0x%llx", frame->rbx);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRCX: 0x%llx", frame->rcx);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRDX: 0x%llx", frame->rdx);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRBP: 0x%llx", frame->rbp);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRSP: 0x%llx", frame->rsp);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRSI: 0x%llx", frame->rsi);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRDI: 0x%llx", frame->rdi);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR8: 0x%llx", frame->r8);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR9: 0x%llx", frame->r9);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR10: 0x%llx", frame->r10);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR11: 0x%llx", frame->r11);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR12: 0x%llx", frame->r12);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR13: 0x%llx", frame->r13);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR14: 0x%llx", frame->r14);
    PRINTLOG(KERNEL, LOG_ERROR, "\tR15: 0x%llx", frame->r15);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRIP: 0x%llx", frame->return_rip);
    PRINTLOG(KERNEL, LOG_ERROR, "\tCS: 0x%x", frame->return_cs);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRFLAGS: 0x%llx", frame->return_rflags);
    PRINTLOG(KERNEL, LOG_ERROR, "\tSS: 0x%x", frame->return_ss);
    PRINTLOG(KERNEL, LOG_ERROR, "\tRSP: 0x%llx", frame->return_rsp);
    PRINTLOG(KERNEL, LOG_ERROR, "\tINT: 0x%llx", frame->interrupt_number);
    PRINTLOG(KERNEL, LOG_ERROR, "\tERROR: 0x%llx", frame->error_code);
}


void interrupt_generic_handler(interrupt_frame_ext_t* frame) {
    uint8_t intnum = frame->interrupt_number;

    if(interrupt_irqs != NULL) {

        if(interrupt_irqs[intnum] != NULL) {
            interrupt_irq_list_item_t* item = interrupt_irqs[intnum];
            uint8_t miss_count = 0;
            boolean_t found = false;

            while(item) {
                interrupt_irq irq = item->irq;

                if(irq != NULL) {
                    int8_t irq_res = irq(frame);

                    if(irq_res != 0) {
                        miss_count++;
                        PRINTLOG(KERNEL, LOG_DEBUG, "irq res status %i for 0x%02x", irq_res, intnum);
                    } else {
                        found = true;
                    }

                } else {
                    PRINTLOG(KERNEL, LOG_FATAL, "null irq at shared irq list for 0x%02x", intnum);
                }

                item = item->next;
            }

            if(!found) {
                PRINTLOG(KERNEL, LOG_WARNING, "cannot find shared irq for 0x%02x miss count 0x%x", intnum, miss_count);
            } else {
                PRINTLOG(KERNEL, LOG_TRACE, "found shared irq for 0x%02x", intnum);

                return;
            }
        }
    } else {
        PRINTLOG(KERNEL, LOG_FATAL, "cannot find irq for 0x%02x", intnum);
    }


    PRINTLOG(KERNEL, LOG_FATAL, "Uncatched interrupt 0x%02x occured without error code.\nReturn address 0x%016llx", intnum, frame->return_rip);
    PRINTLOG(KERNEL, LOG_FATAL, "KERN: FATAL return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);
    PRINTLOG(KERNEL, LOG_FATAL, "cr4: 0x%llx", (cpu_read_cr4()).bits);
    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    cpu_hlt();
}

int8_t interrupt_int01_debug_exception(interrupt_frame_ext_t* frame) {
    KERNEL_PANIC_DISABLE_LOCKS = true;

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    debug_remove_debug_for_address(frame->return_rip);

    KERNEL_PANIC_DISABLE_LOCKS = false;

    return 0;
}

extern boolean_t we_sended_nmi_to_bsp;

int8_t interrupt_int02_nmi_interrupt(interrupt_frame_ext_t* frame) {
    KERNEL_PANIC_DISABLE_LOCKS = true;

    uint32_t apic_id = apic_get_local_apic_id();

    const char_t* return_symbol_name = backtrace_get_symbol_name_by_rip(frame->return_rip);

    if(apic_id == 0 && apic_is_waiting_timer()) {
        video_text_print((char_t*)"bsp stucked, recovering...\n");
        video_text_print((char_t*)"return symbol name: ");
        video_text_print((char_t*)return_symbol_name);
        video_text_print((char_t*)"\n");

        stackframe_t* s_frame = (stackframe_t*)frame->rbp;
        PRINTLOG(KERNEL, LOG_ERROR, "bsp stucked, recovering...");
        PRINTLOG(KERNEL, LOG_ERROR, "return symbol name: %s", return_symbol_name);
        backtrace_print(s_frame);
        KERNEL_PANIC_DISABLE_LOCKS = false;

        frame->interrupt_number = 0x20;

        interrupt_generic_handler(frame);

        return 0;
    }

    PRINTLOG(KERNEL, LOG_FATAL, "NMI interrupt occured at 0x%x:0x%llx %s task 0x%llx", frame->return_cs, frame->return_rip, return_symbol_name, task_get_id());
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    cpu_hlt();

    return -1;
}

int8_t interrupt_int03_breakpoint_exception(interrupt_frame_ext_t* frame) {
    KERNEL_PANIC_DISABLE_LOCKS = true;

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    frame->return_rip--;

    debug_revert_original_byte_at_address(frame->return_rip);

    KERNEL_PANIC_DISABLE_LOCKS = false;

    return 0;
}


int8_t interrupt_int0D_general_protection_exception(interrupt_frame_ext_t* frame){
    KERNEL_PANIC_DISABLE_LOCKS = true;

    const char_t* return_symbol_name = backtrace_get_symbol_name_by_rip(frame->return_rip);

    PRINTLOG(KERNEL, LOG_FATAL, "general protection error 0x%llx at 0x%x:0x%llx %s", frame->error_code, frame->return_cs, frame->return_rip, return_symbol_name);
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);
    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    cpu_hlt();

    return -1;
}

int8_t interrupt_int0E_page_fault_exception(interrupt_frame_ext_t* frame){
    KERNEL_PANIC_DISABLE_LOCKS = true;

    const char_t* return_symbol_name = backtrace_get_symbol_name_by_rip(frame->return_rip);

    PRINTLOG(KERNEL, LOG_FATAL, "page fault occured at 0x%x:0x%llx %s task 0x%llx", frame->return_cs, frame->return_rip, return_symbol_name, task_get_id());
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);

    uint64_t cr2 = cpu_read_cr2();

    interrupt_errorcode_pagefault_t epf = { .bits = (uint32_t)frame->error_code };

    PRINTLOG(KERNEL, LOG_FATAL, "page 0x%016llx P? %i W? %i U? %i I? %i", cr2, epf.fields.present, epf.fields.write, epf.fields.user, epf.fields.instruction_fetch);

    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    cpu_hlt();

    return -1;
}

int8_t interrupt_int13_simd_floating_point_exception(interrupt_frame_ext_t* frame) {
    KERNEL_PANIC_DISABLE_LOCKS = true;

    uint32_t mxcsr = 0;

    asm volatile ("stmxcsr %0" : "=m" (mxcsr));

    const char_t* return_symbol_name = backtrace_get_symbol_name_by_rip(frame->return_rip);

    PRINTLOG(KERNEL, LOG_FATAL, "SIMD exception occured at 0x%x:0x%llx %s task 0x%llx", frame->return_cs, frame->return_rip, return_symbol_name, task_get_id());
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);

    PRINTLOG(KERNEL, LOG_FATAL, "SIMD exception occured at 0x%x:0x%llx task 0x%llx", frame->return_cs, frame->return_rip, task_get_id());
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);

    if(mxcsr & 0x1) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: invalid operation");
    }

    if(mxcsr & 0x2) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: denormalized operand");
    }

    if(mxcsr & 0x4) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: divide by zero");
    }

    if(mxcsr & 0x8) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: overflow");
    }

    if(mxcsr & 0x10) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: underflow");
    }

    if(mxcsr & 0x20) {
        PRINTLOG(KERNEL, LOG_ERROR, "SIMD floating point exception: precision");
    }

    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    stackframe_t* s_frame = (stackframe_t*)frame->rbp;
    backtrace_print_location_and_stackframe_by_rip(frame->return_rip, s_frame);

    interrupt_print_frame_ext(frame);

    cpu_hlt();

    return -1;
}
