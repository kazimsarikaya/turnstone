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

MODULE("turnstone.kernel.cpu.interrupt");

void video_text_print(char_t* string);

//void interrupt_dummy_noerrcode(interrupt_frame_t*, uint8_t);
void interrupt_dummy_errcode(interrupt_frame_t*, interrupt_errcode_t, uint8_t);
void interrupt_register_dummy_handlers(descriptor_idt_t*);

typedef struct interrupt_irq_list_item_t {
    interrupt_irq                     irq;
    struct interrupt_irq_list_item_t* next;
} interrupt_irq_list_item_t;

interrupt_irq_list_item_t** interrupt_irqs = NULL;
uint8_t next_empty_interrupt = 0;

void interrupt_dummy_noerrcode(interrupt_frame_t*, uint8_t);

void __attribute__ ((interrupt)) interrupt_int00_divide_by_zero_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int01_debug_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int02_nmi_interrupt(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int03_breakpoint_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int04_overflow_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int05_bound_range_exceeded_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int06_invalid_opcode_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int07_device_not_available_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int08_double_fault_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int09_coprocessor_segment_overrun(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int0A_invalid_tss_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int0B_segment_not_present(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int0C_stack_fault_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int0D_general_protection_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int0E_page_fault_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int0F_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int10_x87_fpu_error(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int11_alignment_check_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int12_machine_check_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int13_simd_floating_point_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int14_virtualization_exception(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int15_control_protection_exception(interrupt_frame_t*, interrupt_errcode_t);
void __attribute__ ((interrupt)) interrupt_int16_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int17_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int18_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int19_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1A_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1B_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1C_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1D_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1E_reserved(interrupt_frame_t*);
void __attribute__ ((interrupt)) interrupt_int1F_reserved(interrupt_frame_t*);

const uint64_t interrupt_system_defined_interrupts[32] = {
    (uint64_t)&interrupt_int00_divide_by_zero_exception,
    (uint64_t)&interrupt_int01_debug_exception,
    (uint64_t)&interrupt_int02_nmi_interrupt,
    (uint64_t)&interrupt_int03_breakpoint_exception,
    (uint64_t)&interrupt_int04_overflow_exception,
    (uint64_t)&interrupt_int05_bound_range_exceeded_exception,
    (uint64_t)&interrupt_int06_invalid_opcode_exception,
    (uint64_t)&interrupt_int07_device_not_available_exception,
    (uint64_t)&interrupt_int08_double_fault_exception,
    (uint64_t)&interrupt_int09_coprocessor_segment_overrun,
    (uint64_t)&interrupt_int0A_invalid_tss_exception,
    (uint64_t)&interrupt_int0B_segment_not_present,
    (uint64_t)&interrupt_int0C_stack_fault_exception,
    (uint64_t)&interrupt_int0D_general_protection_exception,
    (uint64_t)&interrupt_int0E_page_fault_exception,
    (uint64_t)&interrupt_int0F_reserved,
    (uint64_t)&interrupt_int10_x87_fpu_error,
    (uint64_t)&interrupt_int11_alignment_check_exception,
    (uint64_t)&interrupt_int12_machine_check_exception,
    (uint64_t)&interrupt_int13_simd_floating_point_exception,
    (uint64_t)&interrupt_int14_virtualization_exception,
    (uint64_t)&interrupt_int15_control_protection_exception,
    (uint64_t)&interrupt_int16_reserved,
    (uint64_t)&interrupt_int17_reserved,
    (uint64_t)&interrupt_int18_reserved,
    (uint64_t)&interrupt_int19_reserved,
    (uint64_t)&interrupt_int1A_reserved,
    (uint64_t)&interrupt_int1B_reserved,
    (uint64_t)&interrupt_int1C_reserved,
    (uint64_t)&interrupt_int1D_reserved,
    (uint64_t)&interrupt_int1E_reserved,
    (uint64_t)&interrupt_int1F_reserved
};

boolean_t KERNEL_PANIC_DISABLE_LOCKS = false;

int8_t interrupt_init(void) {
    descriptor_idt_t* idt_table = (descriptor_idt_t*)IDT_REGISTER->base;

    for(int32_t i = 0; i < 32; i++) {
        DESCRIPTOR_BUILD_IDT_SEG(idt_table[i], interrupt_system_defined_interrupts[i], KERNEL_CODE_SEG, 0, DPL_KERNEL)
    }

    interrupt_register_dummy_handlers(idt_table); // 32-255 dummy handlers

    interrupt_irqs = memory_malloc(sizeof(interrupt_irq_list_item_t*) * (255 - 32));

    if(interrupt_irqs == NULL) {
        return -1;
    }

    next_empty_interrupt = INTERRUPT_IRQ_BASE;

    cpu_sti();

    return 0;
}

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

    PRINTLOG(KERNEL, LOG_TRACE, "Setting IRQ handler for IRQ 0x%x func at 0x%p", irqnum, irq);

    cpu_cli();

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


void interrupt_dummy_noerrcode(interrupt_frame_t* frame, uint8_t intnum){

    if(intnum >= 32 && interrupt_irqs != NULL) {
        intnum -= 32;

        if(interrupt_irqs[intnum] != NULL) {
            interrupt_irq_list_item_t* item = interrupt_irqs[intnum];
            uint8_t miss_count = 0;
            boolean_t found = false;

            while(item) {
                interrupt_irq irq = item->irq;

                if(irq != NULL) {
                    int8_t irq_res = irq(frame, intnum);

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

    uint64_t if_addr = (uint64_t)frame;
    backtrace_print((stackframe_t*)(if_addr - 8));

    cpu_hlt();
}

void interrupt_dummy_errcode(interrupt_frame_t* frame, interrupt_errcode_t errcode, uint8_t intnum){
    PRINTLOG(KERNEL, LOG_FATAL, "Uncatched interrupt 0x%02x occured with error code 0x%08llx.\nReturn address 0x%016llx", intnum, errcode, frame->return_rip);
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);
    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    uint64_t if_addr = (uint64_t)frame;
    backtrace_print((stackframe_t*)(if_addr - 8));

    cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int00_divide_by_zero_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x00);
}

void __attribute__ ((interrupt)) interrupt_int01_debug_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x01);
}

extern boolean_t we_sended_nmi_to_bsp;

void __attribute__ ((interrupt)) interrupt_int02_nmi_interrupt(interrupt_frame_t* frame) {
    uint32_t apic_id = apic_get_local_apic_id();

    if(apic_id == 0 && apic_is_waiting_timer()) {
        video_text_print((char_t*)"bsp stucked, recovering...\n");
        interrupt_dummy_noerrcode(frame, 0x20);

        return;
    }

    interrupt_dummy_noerrcode(frame, 0x02);
}

void __attribute__ ((interrupt)) interrupt_int03_breakpoint_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x03);
}

void __attribute__ ((interrupt)) interrupt_int04_overflow_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x04);
}

void __attribute__ ((interrupt)) interrupt_int05_bound_range_exceeded_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x05);
}

void __attribute__ ((interrupt)) interrupt_int06_invalid_opcode_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x06);
}

void __attribute__ ((interrupt)) interrupt_int07_device_not_available_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x07);
}

void __attribute__ ((interrupt)) interrupt_int08_double_fault_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x08);
}

void __attribute__ ((interrupt)) interrupt_int09_coprocessor_segment_overrun(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x09);
}

void __attribute__ ((interrupt)) interrupt_int0A_invalid_tss_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x0A);
}

void __attribute__ ((interrupt)) interrupt_int0B_segment_not_present(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x0B);
}

void __attribute__ ((interrupt)) interrupt_int0C_stack_fault_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x0C);
}

void __attribute__ ((interrupt)) interrupt_int0D_general_protection_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    KERNEL_PANIC_DISABLE_LOCKS = true;
    uint64_t rsp = 0;
    asm volatile ("mov %%rsp, %0\n" : "=r" (rsp));
    stackframe_t* s_frame = backtrace_print_interrupt_registers(rsp);

    PRINTLOG(KERNEL, LOG_FATAL, "general protection error 0x%llx at 0x%x:0x%llx", errcode, frame->return_cs, frame->return_rip);
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);
    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    backtrace_print(s_frame);

    cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int0E_page_fault_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    KERNEL_PANIC_DISABLE_LOCKS = true;
    uint64_t rsp = 0;
    asm volatile ("mov %%rsp, %0\n" : "=r" (rsp));
    stackframe_t* s_frame =  backtrace_print_interrupt_registers(rsp);

    PRINTLOG(KERNEL, LOG_FATAL, "page fault occured at 0x%x:0x%llx task 0x%llx", frame->return_cs, frame->return_rip, task_get_id());
    PRINTLOG(KERNEL, LOG_FATAL, "return stack at 0x%x:0x%llx frm ptr 0x%p", frame->return_ss, frame->return_rsp, frame);

    uint64_t cr2 = cpu_read_cr2();

    interrupt_errorcode_pagefault_t epf = { .bits = (uint32_t)errcode };

    PRINTLOG(KERNEL, LOG_FATAL, "page 0x%016llx P? %i W? %i U? %i I? %i", cr2, epf.fields.present, epf.fields.write, epf.fields.user, epf.fields.instruction_fetch);

    PRINTLOG(KERNEL, LOG_FATAL, "Cpu is halting.");

    backtrace_print(s_frame);

    cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int0F_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x0F);
}

void __attribute__ ((interrupt)) interrupt_int10_x87_fpu_error(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x10);
}

void __attribute__ ((interrupt)) interrupt_int11_alignment_check_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x11);
}

void __attribute__ ((interrupt)) interrupt_int12_machine_check_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x12);
}

void __attribute__ ((interrupt)) interrupt_int13_simd_floating_point_exception(interrupt_frame_t* frame) {
    KERNEL_PANIC_DISABLE_LOCKS = true;
    uint64_t rsp = 0;
    asm volatile ("mov %%rsp, %0\n" : "=r" (rsp));

    uint32_t mxcsr = 0;

    asm volatile ("stmxcsr %0" : "=m" (mxcsr));

    stackframe_t* s_frame =  backtrace_print_interrupt_registers(rsp);

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

    backtrace_print(s_frame);

    cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int14_virtualization_exception(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x14);
}

void __attribute__ ((interrupt)) interrupt_int15_control_protection_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
    interrupt_dummy_errcode(frame, errcode, 0x15);
}

void __attribute__ ((interrupt)) interrupt_int16_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x16);
}

void __attribute__ ((interrupt)) interrupt_int17_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x17);
}

void __attribute__ ((interrupt)) interrupt_int18_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x18);
}

void __attribute__ ((interrupt)) interrupt_int19_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x19);
}

void __attribute__ ((interrupt)) interrupt_int1A_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1A);
}

void __attribute__ ((interrupt)) interrupt_int1B_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1B);
}

void __attribute__ ((interrupt)) interrupt_int1C_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1C);
}

void __attribute__ ((interrupt)) interrupt_int1D_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1D);
}

void __attribute__ ((interrupt)) interrupt_int1E_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1E);
}

void __attribute__ ((interrupt)) interrupt_int1F_reserved(interrupt_frame_t* frame) {
    interrupt_dummy_noerrcode(frame, 0x1F);
}
