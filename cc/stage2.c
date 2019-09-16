asm (".code16gcc");
#include <video.h>

int kmain(void)
{
	set_video();
	clear_screen();
	print("Hello, World!\r\n\0",0,0);
	return 0;
}
