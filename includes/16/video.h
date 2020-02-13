#ifndef ___VIDEO_H
#define ___VIDEO_H 0

#define  VIDEO_BUF_LEN 25*80

void video_print_at(char *, unsigned, unsigned);
void video_print(char *);
void video_clear_screen();
void video_scroll();

#endif
