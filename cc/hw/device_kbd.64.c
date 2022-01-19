/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/kbd.h>
#include <video.h>
#include <cpu.h>
#include <apic.h>

int8_t dev_kbd_isr(interrupt_frame_t* frame, uint8_t intnum){
	UNUSED(frame);
	printf("KEYBOARD: Info keyboard event occured at %i\n", intnum);

	apic_eoi();
	cpu_sti();

	return 0;
}
