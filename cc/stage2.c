asm (".code16gcc");
#include <video.h>
#include <diskio.h>
#include <memory.h>

int kmain(void)
{
	lbasupport ls;
	int status;
	int y = 0;
	set_video();
	clear_screen();
	print("Hello, World!\r\n\0",0,y++);

	init_simple_memory();   //memory init;

	char *data=simple_kmalloc(sizeof(char)*100);
	if(data!=NULL) {
		simple_memset(data,'x',5);
		print(data,0,y++);
	} else {
		return -1;
	}
	char *data2=simple_kmalloc(sizeof(char)*100);
	if(data2!=NULL) {
		simple_memset(data2,'y',5);
		print(data,0,y);
		print(data2,15,y++);
	} else {
		return -1;
	}
	if(data!=NULL) {
		print("free data\0",0,y++);
		if(simple_kfree(data) != 0) {
			print("kfree not ok",15,y++);
			return -1;
		}
		print(data,0,y);
		print(data2,15,y++);
	}
	char *data3=simple_kmalloc(sizeof(char)*200);
	if(data!=NULL) {
		simple_memset(data3,'z',5);
		print(data3,0,y++);
	} else {
		return -1;
	}
	if(data2!=NULL) {
		print("free data2\0",0,y++);
		if(simple_kfree(data2) != 0) {
			print("kfree not ok",15,y++);
			return -1;
		}
		print(data,0,y);
		print(data2,15,y++);
	}
	if(data3!=NULL) {
		print("free data3\0",0,y++);
		if(simple_kfree(data3) != 0) {
			print("kfree not ok",15,y++);
			return -1;
		}
	}

	status = check_lba_support(&ls);
	if(status ==0) {
		print("LBA is supported\r\n",0,y++);
	} else {
		print("LBA is not supported\r\n",0,y++);
	}
	return 0;
}
