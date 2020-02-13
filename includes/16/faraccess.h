#ifndef ___FAR_ACCESS
#define ___FAR_ACCESS 0

static inline char far_read_b(unsigned short seg, unsigned offset){
	char ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %%fs:(%2), %0v\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "r" (offset)
	               );
	return ret;
}

static inline short far_read_w(unsigned short seg, unsigned offset){
	short ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %%fs:(%2), %0\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "r" (offset)
	               );
	return ret;
}

static inline void far_write_b(unsigned short seg, unsigned offset, char data){
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "r" (offset)
	               );
}

static inline void far_write_w(unsigned short seg, unsigned offset, short data){
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "r" (offset)
	               );
}

#endif
