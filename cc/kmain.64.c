#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>

uint8_t kmain64() {
	memory_simple_init();
	video_clear_screen();
	char_t* data = hello_world();
	video_print(data);
	return 0;
}

void __attribute__ ((interrupt)) isr0(void* ctx){

}
