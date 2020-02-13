asm (".code16gcc");

#include <video.h>
#include <memory.h>
#include <faraccess.h>
#include <ports.h>

#define VIDEO_SEG 0xB800
#define WHITE_ON_BLACK  ((0 << 4) | ( 15 & 0x0F))

unsigned cursor_x = 0;
unsigned cursor_y = 0;

void video_clear_screen(){
	int i;
	short blank = ' ' | (WHITE_ON_BLACK <<8);
	for(i=0; i<VIDEO_BUF_LEN; i+=2) {
		far_write_w(VIDEO_SEG, i, blank);
	}
}

void video_print(char *string)
{
	if(string==NULL) {
		return;
	}
	int i=0;
	while(string[i]!='\0') {
		char c = string[i];
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
			unsigned short location = (cursor_y*80+cursor_x)*2;
			far_write_w(VIDEO_SEG, location, c | (WHITE_ON_BLACK <<8));
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
	short cursor = cursor_y*80+cursor_x;
	outb(0x3D4, 14);
	outb(0x3D5, cursor >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, cursor);
}

void video_print_at(char *string, unsigned x, unsigned y) {
	cursor_x = x;
	cursor_y = y;
	video_print(string);
}


void video_scroll() {
	for(int i=0; i<80*24; i++) {
		short data = far_read_w(VIDEO_SEG,(i*2)+160);
		far_write_w(VIDEO_SEG,i*2,data);
	}
	short blank = ' ' | (WHITE_ON_BLACK <<8);
	for(int i = 80*24; i<80*25; i++) {
		far_write_w(VIDEO_SEG, i*2, blank);
	}
}
