#ifndef ___MEMORY_FRAME_H
#define ___MEMORY_FRAME_H 0

#include <types.h>
#include <memory.h>

#define FRAME_SIZE 4096

#define FRAME_ATTRIBUTE_OLD_RESERVED           0x0000000100000000
#define FRAME_ATTRIBUTE_ACPI_RECLAIM_MEMORY    0x0000000200000000

typedef enum {
	FRAME_TYPE_FREE,
	FRAME_TYPE_USED,
	FRAME_TYPE_RESERVED,
	FRAME_TYPE_ACPI_RECLAIM_MEMORY,
} frame_type_t;

typedef enum {
	FRAME_ALLOCATION_TYPE_RELAX = 1 << 1,
	FRAME_ALLOCATION_TYPE_BLOCK = 1 << 2,
	FRAME_ALLOCATION_TYPE_USED = 1 << 7,
	FRAME_ALLOCATION_TYPE_RESERVED = 1 << 8,
	FRAME_ALLOCATION_TYPE_OLD_RESERVED = 1 << 15,
} frame_allocation_type_t;

typedef struct {
	uint64_t frame_address;
	uint64_t frame_count;
	frame_type_t type;
	uint64_t frame_attributes;
} frame_t;


typedef struct frame_allocator {
	void* context;
	int8_t (* allocate_frame_by_count)(struct frame_allocator* self, uint64_t count, frame_allocation_type_t fa_type, frame_t** fs, uint64_t* alloc_list_size);
	int8_t (* allocate_frame)(struct frame_allocator* self, frame_t* f);
	int8_t (* release_frame)(struct frame_allocator* self, frame_t* f);
	frame_t* (* get_reserved_frames_of_address)(struct frame_allocator* self, void* address);
	int8_t (* rebuild_reserved_mmap)(struct frame_allocator* self);
	int8_t (* cleanup)(struct frame_allocator* self);
} frame_allocator_t;

frame_allocator_t* frame_allocator_new_ext(memory_heap_t* heap);
#define frame_allocator_new() frame_allocator_new_ext(NULL);

// TODO: delete me
void frame_allocator_print(frame_allocator_t* fa);

extern frame_allocator_t* KERNEL_FRAME_ALLOCATOR;

#endif
