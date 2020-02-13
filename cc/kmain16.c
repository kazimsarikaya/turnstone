asm (".code16gcc");
#include <video.h>
#include <diskio.h>
#include <memory.h>
#include <ports.h>

int kmain16(void)
{
	lbasupport ls;
	int status;
	video_clear_screen();
	video_print("Hello, World!\r\n");
	init_simple_memory();   //memory init;

	char *data=simple_kmalloc(sizeof(char)*100);
	if(data!=NULL) {
		simple_memset(data,'x',5);
		video_print(data);
	} else {
		return -1;
	}
	char *data2=simple_kmalloc(sizeof(char)*100);
	if(data2!=NULL) {
		simple_memset(data2,'y',5);
		video_print(data);
		video_print(data2);
	} else {
		return -1;
	}
	if(data!=NULL) {
		video_print("free data\0");
		if(simple_kfree(data) != 0) {
			video_print("kfree not ok");
			return -1;
		}
		video_print(data);
		video_print(data2);
	}
	char *data3=simple_kmalloc(sizeof(char)*200);
	if(data!=NULL) {
		simple_memset(data3,'z',5);
		video_print(data3);
	} else {
		return -1;
	}
	if(data2!=NULL) {
		video_print("free data2\0");
		if(simple_kfree(data2) != 0) {
			video_print("kfree not ok");
			return -1;
		}
		video_print(data);
		video_print(data2);
	}
	if(data3!=NULL) {
		video_print("free data3\0");
		if(simple_kfree(data3) != 0) {
			video_print("kfree not ok");
			return -1;
		}
	}

	status = check_lba_support(&ls);
	if(status ==0) {
		video_print("LBA is supported\r\n");
	} else {
		video_print("LBA is not supported\r\n");
	}
	char * alpha = "A\n\0";
	for(int i=0; i< 26; i++) {
		alpha[0] = 65+i;
		video_print(alpha);
	}

	return 0;
}
