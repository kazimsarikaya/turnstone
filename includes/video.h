#ifndef ___VIDEO_H
#define ___VIDEO_H 0

#include <types.h>

#define  VIDEO_BUF_LEN 25*80

void video_print_at(char_t *, uint8_t, uint8_t);
void video_print(char_t *);
void video_clear_screen();
void video_scroll();

#endif
