#include <device/kbd.h>
#include <video.h>
#include <cpu.h>
#include <apic.h>

void dev_kbd_isr(interrupt_frame_t* frame, uint8_t intnum){
	UNUSED(frame);
	printf("KEYBOARD: Info keyboard event occured at %i\n", intnum);

	apic_eoi();
	cpu_sti();
}
