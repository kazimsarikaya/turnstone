/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <cpu/descriptor.h>
#include <cpu/crx.h>
#include <video.h>
#include <strings.h>
#include <memory/paging.h>
#include <memory.h>
#include <apic.h>
#include <cpu/task.h>
#include <backtrace.h>

void interrupt_dummy_noerrcode(interrupt_frame_t*, uint8_t);
void interrupt_dummy_errcode(interrupt_frame_t*, interrupt_errcode_t, uint8_t);
void interrupt_register_dummy_handlers(descriptor_idt_t*);

typedef struct interrupt_irq_list_item {
	interrupt_irq irq;
	struct interrupt_irq_list_item* next;
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

uint64_t interrupt_system_defined_interrupts[32] = {
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

int8_t interrupt_init() {
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

uint8_t interrupt_get_next_empty_interrupt(){
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

int8_t interrupt_irq_set_handler(uint8_t irqnum, interrupt_irq irq) {
	if(interrupt_irqs == NULL) {
		return -1;
	}

	cpu_cli();

	if(interrupt_irqs[irqnum] == NULL) {
		interrupt_irqs[irqnum] = memory_malloc(sizeof(interrupt_irq_list_item_t));
		interrupt_irqs[irqnum]->irq = irq;

	} else {
		interrupt_irq_list_item_t* item = interrupt_irqs[irqnum];

		while(item->next) {
			item = item->next;
		}

		item->next = memory_malloc(sizeof(interrupt_irq_list_item_t));
		item->next->irq = irq;
	}

	cpu_sti();

	return 0;
}


void interrupt_dummy_noerrcode(interrupt_frame_t* frame, uint8_t intnum){

	if(intnum >= 32 && interrupt_irqs != NULL) {
		intnum -= 32;

		if(interrupt_irqs[intnum] != NULL) {
			interrupt_irq_list_item_t* item = interrupt_irqs[intnum];
			uint8_t miss_count = 0;

			while(item) {
				interrupt_irq irq = item->irq;

				if(irq != NULL) {
					int8_t irq_res = irq(frame, intnum);

					if(irq_res == 0) {
						return;
					} else {
						PRINTLOG(KERNEL, LOG_WARNING, "irq res status %i for 0x%02x", irq_res, intnum);
					}

				} else {
					PRINTLOG(KERNEL, LOG_FATAL, "null irq at shared irq list for 0x%02x", intnum);
				}

				miss_count++;
				item = item->next;
			}

			PRINTLOG(KERNEL, LOG_WARNING, "cannot find shared irq for 0x%02x miss count 0x%x", intnum, miss_count);

			return;
		} else {
			PRINTLOG(KERNEL, LOG_FATAL, "cannot find irq for 0x%02x", intnum);
		}
	}

	printf("Uncatched interrupt 0x%02x occured without error code.\nReturn address 0x%016llx\n", intnum, frame->return_rip);
	printf("KERN: FATAL return stack at 0x%x:0x%llx frm ptr 0x%p\n", frame->return_ss, frame->return_rsp, frame);
	printf("cr4: 0x%llx\n", (cpu_read_cr4()).bits);
	printf("Cpu is halting.");

	backtrace_print((stackframe_t*)frame->return_rsp);

	cpu_hlt();
}

void interrupt_dummy_errcode(interrupt_frame_t* frame, interrupt_errcode_t errcode, uint8_t intnum){
	printf("Uncatched interrupt 0x%02x occured with error code 0x%08llx.\nReturn address 0x%016llx\n", intnum, errcode, frame->return_rip);
	printf("KERN: FATAL return stack at 0x%x:0x%llx frm ptr 0x%p\n", frame->return_ss, frame->return_rsp, frame);
	printf("Cpu is halting.");

	backtrace_print((stackframe_t*)frame->return_rsp);

	cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int00_divide_by_zero_exception(interrupt_frame_t* frame) {
	interrupt_dummy_noerrcode(frame, 0x00);
}

void __attribute__ ((interrupt)) interrupt_int01_debug_exception(interrupt_frame_t* frame) {
	interrupt_dummy_noerrcode(frame, 0x01);
}

void __attribute__ ((interrupt)) interrupt_int02_nmi_interrupt(interrupt_frame_t* frame) {
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
	printf("\nKERN: FATAL general protection error 0x%llx at 0x%x:0x%llx\n", errcode, frame->return_cs, frame->return_rip);
	printf("KERN: FATAL return stack at 0x%x:0x%llx frm ptr 0x%p\n", frame->return_ss, frame->return_rsp, frame);
	printf("Cpu is halting.");

	backtrace_print((stackframe_t*)frame->return_rsp);

	cpu_hlt();
}

void __attribute__ ((interrupt)) interrupt_int0E_page_fault_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
	printf("\nKERN: INFO page fault occured with code 0x%016llx at 0x%016llx task 0x%llx\n", errcode, frame->return_rip, task_get_id());
	printf("KERN: FATAL return stack at 0x%x:0x%llx frm ptr 0x%p\n", frame->return_ss, frame->return_rsp, frame);

	uint64_t cr2 = cpu_read_cr2();

	printf("\nKERN: INFO page does not exists for address 0x%016llx\n", cr2 );

	printf("Cpu is halting.");

	backtrace_print((stackframe_t*)frame->return_rsp);

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
	interrupt_dummy_noerrcode(frame, 0x13);
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
