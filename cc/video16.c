asm (".code16gcc");

#include <video.h>
#include <memory.h>
#include <faraccess.h>
#include <ports.h>

#define VIDEO_SEG 0xB800
#define WHITE_ON_BLACK  ((0 << 4) | ( 15 & 0x0F))

uint16_t cursor_x = 0;
uint16_t cursor_y = 0;

void video_clear_screen(){
	size_t i;
	uint16_t blank = ' ' | (WHITE_ON_BLACK <<8);
	for(i=0; i<VIDEO_BUF_LEN; i+=2) {
		far_write_16(VIDEO_SEG, i, blank);
	}
}

void video_print(char_t *string)
{
	if(string==NULL) {
		return;
	}
	size_t i=0;
	while(string[i]!='\0') {
		char_t c = string[i];
		if( c == '\r') {
			cursor_x = 0;
		} else if ( c == '\n') {
			cursor_x =0;
			cursor_y++;
			if (cursor_y >= 25) {
				cursor_y = 24;
				video_scroll();
			}
		} else if ( c >= ' ') {
			uint16_t location = (cursor_y*80+cursor_x)*2;
			far_write_16(VIDEO_SEG, location, c | (WHITE_ON_BLACK <<8));
			cursor_x++;
			if(cursor_x == 80) {
				cursor_x = 0;
				cursor_y++;
				if (cursor_y >= 25) {
					cursor_y = 24;
					video_scroll();
				}
			}
		}
		i++;
	}
	uint16_t cursor = cursor_y*80+cursor_x;
	outb(0x3D4, 14);
	outb(0x3D5, cursor >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, cursor);
}

void video_print_at(char_t *string, uint8_t x, uint8_t y) {
	cursor_x = x;
	cursor_y = y;
	video_print(string);
}


void video_scroll() {
	for(size_t i=0; i<80*24; i++) {
		uint16_t data = far_read_16(VIDEO_SEG,(i*2)+160);
		far_write_16(VIDEO_SEG,i*2,data);
	}
	uint16_t blank = ' ' | (WHITE_ON_BLACK <<8);
	for(size_t i = 80*24; i<80*25; i++) {
		far_write_16(VIDEO_SEG, i*2, blank);
	}
}
