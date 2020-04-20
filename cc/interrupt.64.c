#include <types.h>
#include <interrupt.h>
#include <cpu.h>
#include <descriptor.h>
#include <video.h>
#include <strings.h>

void __attribute__ ((interrupt)) interrupt_dummy_noerrcode(interrupt_frame_t* frame);
void __attribute__ ((interrupt)) interrupt_dummy_errcode(interrupt_frame_t* frame, interrupt_errcode_t errcode);

void interrupt_init() {
	descriptor_idt_t* idt_table = (descriptor_idt_t*)IDT_REGISTER.base;

	uint64_t dummy_i_h_nec = (uint64_t)&interrupt_dummy_noerrcode;
	for(uint16_t i = 32; i<256; i++) {
		DESCRIPTOR_BUILD_IDT_SEG(idt_table[i], dummy_i_h_nec, KERNEL_CODE_SEG, 0, DPL_KERNEL);
	}
	cpu_sti();
}


void __attribute__ ((interrupt)) interrupt_dummy_noerrcode(interrupt_frame_t* frame){
	cpu_cli();
	video_print("Uncached interrupt occured without error code.\r\nReturn address: \0");
	video_print(itoh(frame->return_rip));
	video_print("\r\nCpu is halting.\0");
	cpu_halt();
}
void __attribute__ ((interrupt)) interrupt_dummy_errcode(interrupt_frame_t* frame, interrupt_errcode_t errcode){
	cpu_cli();
	video_print("Uncached interrupt occured with error code.\r\nReturn address: \0");
	video_print(itoh(frame->return_rip));
	video_print("\r\nError code: \0");
	video_print(itoh(errcode));
	video_print("\r\nCpu is halting.\0");
	cpu_halt();
}
