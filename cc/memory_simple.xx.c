/**
 * @file memory_simple.xx.c
 * @brief a simple heap implementation
 */
#include <memory.h>
#include <systeminfo.h>
#include <cpu.h>

/*! heap flag for heap start and end hi */
#define HEAP_INFO_FLAG_STARTEND        (1 << 0)
/*! used heap hole flag */
#define HEAP_INFO_FLAG_USED            (1 << 1)
/*! unused heap hole flag */
#define HEAP_INFO_FLAG_NOTUSED         (0 << 1)
/*! heap metadata flag */
#define HEAP_INFO_FLAG_HEAP            (1 << 4)
/*! heap initilized flag */
#define HEAP_INFO_FLAG_HEAP_INITED     (HEAP_INFO_FLAG_HEAP | HEAP_INFO_FLAG_USED)
/*! heap magic for protection*/
#define HEAP_INFO_MAGIC                (0xaa55)
/*! heap padding for protection */
#define HEAP_INFO_PADDING              (0xB16B00B5)
/*! heap header */
#define HEAP_HEADER                    HEAP_INFO_PADDING


/*! heap bottom address comes frome linker script */
extern size_t __kheap_bottom;

/**
 * @struct __heapinfo
 * @brief heap info struct
 */
typedef struct __heapinfo {
	uint16_t magic; ///< magic value of heap for protection
	uint16_t flags; ///< flags of hi
	struct __heapinfo* next; ///< next hi node
	struct __heapinfo* previous; ///< previous hi node
	uint32_t padding; ///< for 8 byte align for protection
}__attribute__ ((packed)) heapinfo_t; ///< short hand for struct

/**
 * @brief simple heap malloc implementation
 * @param[in] heap  simple heap (itself)
 * @param[in] size  size of memory
 * @param[in] align align value
 * @return allocated memory start address
 */
void* memory_simple_malloc_ext(memory_heap_t* heap, size_t size, size_t align);

/**
 * @brief simple heap free implementation
 * @param[in]  heap simple heap (itself)
 * @param[in]  address address to free
 * @return         0 if successed
 */
uint8_t memory_simple_free(memory_heap_t* heap, void* address);

memory_heap_t* memory_create_heap_simple(size_t start, size_t end){
	size_t heap_start, heap_end;
	if(start == 0 && end == 0) {
		heap_start = (size_t)&__kheap_bottom;
#if ___BITS == 16
		heap_end = 0xFFFF;
#elif ___BITS == 64
		memory_map_t* mmap = SYSTEM_INFO->mmap;
		while(mmap->type != 1) {
			mmap++;
		}
		heap_end =  mmap->base + mmap->length;
#endif
	}
	else if(start == 0 || end == 0) {
		return NULL;
	} else {
		heap_start = start;
		heap_end = end;
	}
	memory_heap_t* heap = (memory_heap_t*)(heap_start);
	size_t metadata_start = heap_start + sizeof(memory_heap_t);
	heapinfo_t* metadata = (heapinfo_t*)(metadata_start);
	heap->header = HEAP_HEADER;
	heap->metadata = metadata;
	heap->malloc = &memory_simple_malloc_ext;
	heap->free = &memory_simple_free;

	if((metadata->flags & HEAP_INFO_FLAG_HEAP_INITED) == HEAP_INFO_FLAG_HEAP_INITED) {
		return heap;
	}
	metadata->previous = (heapinfo_t*)(metadata_start + sizeof(heapinfo_t));
	metadata->next = (heapinfo_t*)(heap_end - sizeof(heapinfo_t));
	metadata->flags = HEAP_INFO_FLAG_HEAP;
	metadata->magic = HEAP_INFO_MAGIC;
	metadata->padding = HEAP_INFO_PADDING;

	heapinfo_t* hibottom = metadata->previous;
	heapinfo_t* hitop = metadata->next;

	hibottom->magic = HEAP_INFO_MAGIC;
	hibottom->padding = HEAP_INFO_PADDING;
	hitop->magic = HEAP_INFO_MAGIC;
	hitop->padding = HEAP_INFO_PADDING;

	hibottom->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_NOTUSED; // heapstart, empty;
	hitop->flags = HEAP_INFO_FLAG_STARTEND | HEAP_INFO_FLAG_USED; //heaptop, full;

	hibottom->next = hitop;
	hibottom->previous = NULL;

	hitop->next = NULL;
	hitop->previous =  hibottom;
	metadata->flags |= HEAP_INFO_FLAG_USED;

	return heap;
}

void* memory_simple_malloc_ext(memory_heap_t* heap, size_t size, size_t align){
	heapinfo_t* simple_heap = (heapinfo_t*)heap->metadata;

	heapinfo_t* hibottom = simple_heap->previous;
	heapinfo_t* hitop = simple_heap->next;

	heapinfo_t* hi = hibottom;
	size_t a_size = size + align;
	size_t t_size =  a_size / sizeof(heapinfo_t);
	if ((a_size % sizeof(heapinfo_t)) != 0) {
		t_size++;
	}

	//find first empty and enough slot
	while( (hi != hitop) &&
	       ((hi->flags & HEAP_INFO_FLAG_USED) ==  HEAP_INFO_FLAG_USED// empty?
	        || hi + 1 + t_size > hi->next)) { // size enough?
		hi = hi->next;
	}
	if( hi == hitop) {
		return NULL;
	}

	heapinfo_t* tmp = hi + 1 + t_size;
	if(tmp != hi->next) {
		tmp->magic =  HEAP_INFO_MAGIC;
		tmp->padding = HEAP_INFO_PADDING;
		tmp->flags = HEAP_INFO_FLAG_NOTUSED;
		tmp->previous = hi;
		tmp->next = hi->next;
		hi->next->previous = tmp;
		hi->next = tmp;
	}

	if(align > 0) {
		size_t hi_addr = (size_t)hi;
		size_t tmp_addr = (size_t)tmp;
		size_t free_addr = hi_addr + sizeof(heapinfo_t);
		size_t aligned_addr = ((free_addr / align) + 1) * align;
		size_t hi_aligned_addr = aligned_addr - sizeof(heapinfo_t);
		size_t left_diff = hi_aligned_addr - hi_addr;
		size_t right_diff = tmp_addr - (aligned_addr + size);

		heapinfo_t* hi_r;
		heapinfo_t* hi_a;

		// fix right empty area
		if(right_diff > 0) {
			heapinfo_t* tmp2 = tmp->next;
			memory_memclean(tmp, sizeof(heapinfo_t));
			hi_r = (heapinfo_t*)(aligned_addr + size);
			hi_r->magic = HEAP_INFO_MAGIC;
			hi_r->padding = HEAP_INFO_PADDING;
			hi_r->flags = HEAP_INFO_FLAG_NOTUSED;
			hi_r->next = tmp2;
			tmp2->previous = hi_r;
			tmp = hi_r;
		}



		// fix left empty area, if not so small add new heapinfo
		// if so small move hi to hi_aligned
		hi_a = (heapinfo_t*)hi_aligned_addr;
		if(left_diff > sizeof(heapinfo_t)) {
			hi_a->previous = hi;
			hi->next = hi_a;
		} else {
			//store hi prev to rebuild hi aka hi_a if needed
			heapinfo_t* hi_prev = hi->previous;
			memory_memclean(hi, sizeof(heapinfo_t));
			hi_a->previous = hi_prev;
			hi_prev->next = hi_a;
		}
		hi_a->next = tmp;
		tmp->previous = hi_a;
		hi_a->magic = HEAP_INFO_MAGIC;
		hi_a->padding = HEAP_INFO_PADDING;
		hi_a->flags = HEAP_INFO_FLAG_USED;

		return (uint8_t*)aligned_addr;
	} else {
		hi->flags |= HEAP_INFO_FLAG_USED;
		memory_memclean(hi + 1, size);
		return hi + 1;
	}
}


uint8_t memory_simple_free(memory_heap_t* heap, void* address){
	if(address == NULL) {
		return -1;
	}
	heapinfo_t* hi = ((heapinfo_t*)address) - 1;
	if(hi->magic != HEAP_INFO_MAGIC && hi->padding != HEAP_INFO_PADDING) {
		//incorrect address to clean
		return -1;
	}

	if(hi->next == NULL) {
		// heap end, can not clean
		return -1;
	}
	hi->flags ^= HEAP_INFO_FLAG_USED;


	//clean data
	size_t size = (void*)hi->next - address;
	memory_memclean(address, size);

	//merge next empty slot if any
	if(!(hi->next->flags & HEAP_INFO_FLAG_USED)) {
		void* caddr = hi->next;
		hi->next = hi->next->next; //next cannot be NULL do not afraid of it
		hi->next->previous = hi;
		memory_memclean(caddr, sizeof(heapinfo_t));
	}

	// merge previous empty slot if any
	if(hi->previous != NULL) {
		if(!(hi->previous->flags & HEAP_INFO_FLAG_USED)) { // if prev is full then stop
			void* caddr = hi;
			hi->previous->next = hi->next;
			hi->next->previous = hi->previous;
			memory_memclean(caddr, sizeof(heapinfo_t));
		}
	}
	return 0;
}
