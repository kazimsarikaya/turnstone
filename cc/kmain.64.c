#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <strings.h>
#include <interrupt.h>

uint8_t kmain64() {
	memory_simple_init();
	interrupt_init();
	video_clear_screen();
	char_t* data = hello_world();
	video_print(data);

	asm ("int $0x80");

	return 0;
}
