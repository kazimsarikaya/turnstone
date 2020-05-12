#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <strings.h>
#include <cpu/interrupt.h>

uint8_t kmain64() {
	memory_heap_t* heap = memory_create_heap_simple(0, 0);
	memory_set_default_heap(heap);
	memory_page_table_t* p4 = memory_paging_clone_pagetable();
	memory_paging_switch_table(p4);

	interrupt_init();
	video_clear_screen();
	char_t* data = hello_world();
	video_print(data);

	asm ("int $0x80");

	return 0;
}
