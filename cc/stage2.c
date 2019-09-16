asm (".code16gcc");
#include <video.h>
#include <diskio.h>

int kmain(void)
{
	lbasupport ls;
	int status;
	set_video();
	clear_screen();
	print("Hello, World!\r\n\0",0,0);
	status = check_lba_support(&ls);
	if(status ==0) {
		print("LBA is supported\r\n",0,1);
	} else {
		print("LBA is not supported\r\n",0,1);
	}
	return 0;
}
