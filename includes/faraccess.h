#ifndef ___FAR_ACCESS_H
#define ___FAR_ACCESS_H 0

#include <types.h>

static inline uint8_t far_read_8(reg_t seg, reg_t offset){
#if ___BITS == 16
	uint8_t ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %%fs:(%2), %0v\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "b" (offset)
	               );
	return ret;
#elif ___BITS == 64
	uint8_t* ret = (uint8_t*)(seg * 16 + offset);
	return *ret;
#endif
}

static inline uint16_t far_read_16(reg_t seg, reg_t offset){
#if ___BITS == 16
	short ret;
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %%fs:(%2), %0\n"
	               "pop %%fs\n"
	               : "=r" (ret) : "r" (seg), "b" (offset)
	               );
	return ret;
#elif ___BITS == 64
	uint16_t* ret = (uint16_t*)(seg * 16 + offset);
	return *ret;
#endif
}

static inline void far_write_8(reg_t seg, reg_t offset, uint8_t data){
#if ___BITS == 16
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movb %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "b" (offset)
	               );
#elif ___BITS == 64
	uint8_t* ret = (uint8_t*)(seg * 16 + offset);
	*ret = data;
#endif
}

static inline void far_write_16(reg_t seg, reg_t offset, uint16_t data){
#if ___BITS == 16
	asm volatile ( "push %%fs\n"
	               "movw %1, %%fs\n"
	               "movw %0, %%fs:(%2)\n"
	               "pop %%fs\n"
	               : : "r" (data), "r" (seg), "b" (offset)
	               );
#elif ___BITS == 64
	uint16_t* ret = (uint16_t*)(seg * 16 + offset);
	*ret = data;
#endif
}
#endif
