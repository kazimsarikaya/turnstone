#include <video.h>
#include <diskio.h>
#include <memory.h>
#include <ports.h>
#include <strings.h>
#include <utils.h>

uint8_t kmain16(void)
{
	init_simple_memory();
	video_clear_screen();
	video_print("Hello, World!\r\n");

	int16_t i = 1234;
	char* data10=itoa(i);
	char* data16=itoh(i);
	video_print(data10);
	video_print("  ");
	video_print(data16);
	video_print("\n");

	if( 729 != power(3,6)) {
		video_print("power error\n");
	} else {
		video_print("power ok\n");
	}

	if (strlen(data10) != 4) {
		video_print("strlen error\n");
	} else {
		video_print("strlen ok\n");
	}

	if (strlen(data16) != 3) {
		video_print("strlen error\n");
	} else {
		video_print("strlen ok\n");
	}

	if (strcmp(data10,"1234") != 0) {
		video_print("strcmp error\n");
	} else {
		video_print("strcmp ok\n");
	}

	if (strcmp("data10","1234") != 1) {
		video_print("strcmp error\n");
	} else {
		video_print("strcmp ok\n");
	}

	char * sr_test = strrev("1234");
	if(strcmp(sr_test,"4321")) {
		video_print("strrev error\n");
	} else {
		video_print("strrev ok\n");
	}
	simple_kfree(sr_test);

	if (i == atoi(data10)) {
		video_print("OK10  ");
	}else {
		video_print("NOK10 ");
	}

	if (i == atoh(data16)) {
		video_print("OK16");
	}else {
		video_print("NOK16 ");
	}
	simple_kfree(data10);
	simple_kfree(data16);

	init_serial(COM1);

	char_t * sdata = "Hello\r\nWorld";
	char_t r = read_serial(COM1);
	write_serial(COM1,r);
	for(i=0; i<strlen(sdata); i++) {
		write_serial(COM1,sdata[i]);
	}


	return 0;
}
