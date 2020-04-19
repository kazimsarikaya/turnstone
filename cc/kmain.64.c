#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <strings.h>

uint8_t kmain64() {
	memory_simple_init();
	video_clear_screen();
	char_t* data = hello_world();
	video_print(data);

	uint8_t* data1 = memory_simple_kmalloc(sizeof(uint8_t) * 10);
	data1[0] = 'A';
	uint8_t* data2 = memory_simple_kmalloc(sizeof(uint8_t) * 200);
	data2[0] = 'B';
	uint8_t* data3 = memory_simple_kmalloc(sizeof(uint8_t) * 32);
	data3[0] = 'C';
	memory_simple_kfree(data2);
	uint8_t* data4 = memory_simple_kmalloc(sizeof(uint8_t) * 32);
	data4[0] = 'D';
	uint8_t* data5 = memory_simple_kmalloc(sizeof(uint8_t) * 200);
	data5[0] = 'E';


	return 0;
}

void __attribute__ ((interrupt)) isr0(void* ctx){

}
