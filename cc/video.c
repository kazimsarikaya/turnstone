asm (".code16gcc");

#include <video.h>
#include <memory.h>

void set_video(){
	asm ("mov $0x0003, %ax\n"
	     "int $0x10");
}

void clear_screen(){
	int i;
	for(i=0; i<VIDEO_BUF_LEN; i++) {
		asm volatile ("int $0x10" : : "a" (0x0e00 | ' '), "b" (0x0007));
	}
}

void print(char *string, int x, int y)
{
	if(string==NULL) {
		return;
	}
	int i=0;
	asm volatile ("int $0x10" : : "a" (0x0200), "b" (0), "d" (y<<8|x));
	while(string[i]!='\0') {
		asm volatile ("int $0x10" : : "a" (0x0e00 | string[i]), "b" (0x0007));
		i++;
	}
}
