asm (".code16gcc");

static const int VIDEO_BUF_LEN=25*80;

static void set_video(){
	asm ("mov $0x0003, %ax\n"
	     "int $0x10");
}

static void clear_screen(){
	int i;
	for(i=0; i<VIDEO_BUF_LEN; i++) {
		asm ("int $0x10" : : "a" (0x0e00 | ' '), "b" (0x0007));
	}
}

static void print(char *string, int x, int y)
{
	int i=0;
	asm ("int $0x10" : : "a" (0x0200), "b" (0), "d" (y<<8|x));
	while(string[i]!='\0') {
		asm ("int $0x10" : : "a" (0x0e00 | string[i]), "b" (0x0007));
		i++;
	}
}

int kmain(void)
{
	set_video();
	clear_screen();
	print("Hello, World!\r\n\0",0,0);
	return 0;
}
