#ifndef ___FAR_ACCESS
#define ___FAR_ACCESS 0

#include <types.h>

static inline uint8_t far_read_8(reg_t seg, reg_t offset){
	uint8_t ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %%fs:(%2), %0v\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "b" (offset)
	               );
	return ret;
}

static inline uint16_t far_read_16(reg_t seg, reg_t offset){
	short ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %%fs:(%2), %0\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "b" (offset)
	               );
	return ret;
}

static inline void far_write_8(reg_t seg, reg_t offset, uint8_t data){
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "b" (offset)
	               );
}

static inline void far_write_16(reg_t seg, reg_t offset, uint16_t data){
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "b" (offset)
	               );
}

#endif
