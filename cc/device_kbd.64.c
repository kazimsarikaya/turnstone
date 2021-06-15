#include <device/kbd.h>
#include <video.h>

void dev_kbd_isr(interrupt_frame_t* frame, uint16_t intnum){
	UNUSED(frame);
	printf("KEYBOARD: Info keyboard event occured at %i\n", intnum);
}
