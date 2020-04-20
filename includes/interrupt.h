#ifndef ___INTERRUPT_H
#define ___INTERRUPT_H 0

#if ___BITS == 64

#define interrupt_errcode_t uint64_t

typedef struct interrupt_frame {
	uint64_t return_rip;
	uint16_t return_cs;
	uint64_t empty1 : 48;
	uint64_t return_rflags;
	uint64_t return_rsp;
	uint16_t return_ss;
	uint64_t empty2 : 48;
} __attribute__((packed)) interrupt_frame_t;

void interrupt_init();

#endif

#endif
