#include <types.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <cpu/descriptor.h>
#include <video.h>
#include <strings.h>


void interrupt_dummy_noerrcode(interrupt_frame_t*, uint16_t);
void interrupt_dummy_errcode(interrupt_frame_t*, interrupt_errcode_t, uint16_t);
void interrupt_register_dummy_handlers(descriptor_idt_t*);

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
void __attribute__ ((interrupt)) interrupt_int13_simd_floaring_point_exception(interrupt_frame_t*);
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
	(uint64_t)&interrupt_int13_simd_floaring_point_exception,
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

void interrupt_init() {
	descriptor_idt_t* idt_table = (descriptor_idt_t*)IDT_REGISTER.base;

	for(number_t i = 0; i < 32; i++) {
		DESCRIPTOR_BUILD_IDT_SEG(idt_table[i], interrupt_system_defined_interrupts[i], KERNEL_CODE_SEG, 0, DPL_KERNEL)
	}

	interrupt_register_dummy_handlers(idt_table); // 32-255 dummy handlers
	cpu_sti();
}


void interrupt_dummy_noerrcode(interrupt_frame_t* frame, uint16_t intnum){
	cpu_cli();
	video_print("Uncached interrupt occured without error code.\0");
	video_print("\r\nInterrupt number: \0");
	video_print(itoh(intnum));
	video_print("\r\nReturn address: \0");
	video_print(itoh(frame->return_rip));
	video_print("\r\nCpu is halting.\0");
	cpu_hlt();
}

void interrupt_dummy_errcode(interrupt_frame_t* frame, interrupt_errcode_t errcode, uint16_t intnum){
	cpu_cli();
	video_print("Uncached interrupt occured with error code.\0");
	video_print("\r\nInterrupt number: \0");
	video_print(itoh(intnum));
	video_print("\r\nReturn address: \0");
	video_print(itoh(frame->return_rip));
	video_print("\r\nError code: \0");
	video_print(itoh(errcode));
	video_print("\r\nCpu is halting.\0");
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
	interrupt_dummy_errcode(frame, errcode, 0x0D);
}

void __attribute__ ((interrupt)) interrupt_int0E_page_fault_exception(interrupt_frame_t* frame, interrupt_errcode_t errcode){
	interrupt_dummy_errcode(frame, errcode, 0x0E);
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

void __attribute__ ((interrupt)) interrupt_int13_simd_floaring_point_exception(interrupt_frame_t* frame) {
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


// the fucking ugly code generated by m4
#ifndef ___DEPEND_ANALYSIS
#include "../cc-gen/interrupt_handlers.c"
#endif
