#include <memory.h>
#include <systeminfo.h>
#include <cpu.h>

#define HEAP_INFO_FLAG_STARTEND        (1 << 0)
#define HEAP_INFO_FLAG_USED            (1 << 1)
#define HEAP_INFO_FLAG_NOTUSED         (0 << 1)
#define HEAP_INFO_FLAG_HEAP            (1 << 4)
#define HEAP_INFO_FLAG_HEAP_INITED     (HEAP_INFO_FLAG_HEAP | HEAP_INFO_FLAG_USED)
#define HEAP_INFO_MAGIC                (0xaa55)
#define HEAP_INFO_PADDING              (0xB16B00B5)


extern size_t __kheap_bottom;

typedef struct __heapinfo {
	uint16_t magic;
	uint16_t flags;
	struct __heapinfo* next;
	struct __heapinfo* previous;
	uint32_t padding; // for 8 byte align
}__attribute__ ((packed)) heapinfo_t;

memory_simple_heap_t memory_simple_heap_default;

memory_simple_heap_t memory_simple_create_heap(size_t start, size_t end) {
	heapinfo_t* heap = (heapinfo_t*)(start);
	if((heap->flags & HEAP_INFO_FLAG_HEAP_INITED) == HEAP_INFO_FLAG_HEAP_INITED) {
		return heap;
	}
	heap->previous = (heapinfo_t*)(start + sizeof(heapinfo_t));
	heap->next = (heapinfo_t*)(end - sizeof(heapinfo_t));
	heap->flags = HEAP_INFO_FLAG_HEAP;
	heap->magic = HEAP_INFO_MAGIC;
	heap->padding = HEAP_INFO_PADDING;

	return heap;
}

uint8_t memory_simple_init_ext(memory_simple_heap_t ptrheap){
	heapinfo_t* heap = (heapinfo_t*)ptrheap;
	if(heap == NULL) {
#if ___BITS == 16
		heap = memory_simple_create_heap((size_t)&__kheap_bottom, 0xFFFF);
#elif ___BITS == 64
		memory_map_t* mmap = SYSTEM_INFO->mmap;
		while(mmap->type != 1) {
			mmap++;
		}
		heap = memory_simple_create_heap((size_t)&__kheap_bottom, mmap->base + mmap->length);
#endif
		memory_simple_heap_default = heap;
	}
	if((heap->flags & HEAP_INFO_FLAG_HEAP_INITED) == HEAP_INFO_FLAG_HEAP_INITED) {
		return 0;
	}

	heapinfo_t* hibottom = heap->previous;
	heapinfo_t* hitop = heap->next;

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
	heap->flags |= HEAP_INFO_FLAG_USED;

	return 0;
}

void* memory_simple_kmalloc_ext(memory_simple_heap_t ptrheap, size_t size, size_t align){
	heapinfo_t* heap = (heapinfo_t*)ptrheap;
	if(heap == NULL) {
		heap = memory_simple_heap_default;
	}

	heapinfo_t* hibottom = heap->previous;
	heapinfo_t* hitop = heap->next;

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
			memory_simple_memclean(tmp, sizeof(heapinfo_t));
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
			memory_simple_memclean(hi, sizeof(heapinfo_t));
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
		memory_simple_memclean(hi + 1, size);
		return hi + 1;
	}
}


uint8_t memory_simple_kfree(void* address){
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
	memory_simple_memclean(address, size);

	//merge next empty slot if any
	if(!(hi->next->flags & HEAP_INFO_FLAG_USED)) {
		void* caddr = hi->next;
		hi->next = hi->next->next; //next cannot be NULL do not afraid of it
		hi->next->previous = hi;
		memory_simple_memclean(caddr, sizeof(heapinfo_t));
	}

	// merge previous empty slot if any
	if(hi->previous != NULL) {
		if(!(hi->previous->flags & HEAP_INFO_FLAG_USED)) { // if prev is full then stop
			void* caddr = hi;
			hi->previous->next = hi->next;
			hi->next->previous = hi->previous;
			memory_simple_memclean(caddr, sizeof(heapinfo_t));
		}
	}
	return 0;
}

void memory_simple_memset(void* address, uint8_t value, size_t size) {
	for(size_t i = 0; i < size; i++) {
		((uint8_t*)address)[i] = value;
	}
}

void memory_simple_memcpy(void* source, void* destination, size_t length){
	uint8_t* src = (uint8_t*)source;
	uint8_t* dst = (uint8_t*)destination;
	for(size_t i = 0; i < length; i++) {
		dst[i] = src[i];
	}
}

size_t memory_detect_map(memory_map_t** mmap) {
	*mmap = memory_simple_kmalloc(sizeof(memory_map_t) * MEMORY_MMAP_MAX_ENTRY_COUNT);
	memory_map_t* mmap_a = *mmap;
	regext_t contID = 0, signature, bytes;
	size_t entries = 0;
	do {
		__asm__ __volatile__ ("int $0x15"
		                      : "=a" (signature), "=c" (bytes), "=b" (contID)
		                      : "a" (0xE820), "b" (contID), "c" (24), "d" (0x534D4150), "D" (mmap_a));
		if(signature != 0x534D4150) {
			return -1;
		}
		if(bytes > 20 && mmap_a->acpi & 0x0001) {

		}
		else{
			mmap_a++;
			entries++;
		}
	} while(contID != 0 && entries < MEMORY_MMAP_MAX_ENTRY_COUNT);

	memory_map_t* mmap_r = memory_simple_kmalloc(sizeof(memory_map_t) * entries);
	memory_simple_memcpy(*mmap, mmap_r, sizeof(memory_map_t) * entries);
	memory_simple_kfree(*mmap);
	*mmap = mmap_r;

	return entries;
}

size_t memory_get_absolute_address(size_t raddr) {
#if ___BITS == 16
	size_t ds = cpu_read_data_segment();
	ds <<= 4;
	ds += raddr;
	return ds;
#endif
	return raddr;
}

size_t memory_get_relative_address(size_t raddr) {
#if ___BITS == 16
	size_t ds = cpu_read_data_segment();
	ds <<= 4;
	ds = raddr - ds;
	return ds;
#endif
	return raddr;
}

uint8_t memory_build_page_table(){
	page_table_t* p4 = memory_simple_kmalloc_aligned(sizeof(page_table_t), 0x1000);
	if(p4 == NULL) {
		return -1;
	}
	page_table_t* p3 = memory_simple_kmalloc_aligned(sizeof(page_table_t), 0x1000);
	if(p3 == NULL) {
		return -1;
	}
	page_table_t* p2 = memory_simple_kmalloc_aligned(sizeof(page_table_t), 0x1000);
	if(p2 == NULL) {
		return -1;
	}

	p4->pages[0].present = 1;
	p4->pages[0].writable = 1;

	size_t p3_addr = memory_get_absolute_address((size_t)p3);
#if ___BITS == 16
	p4->pages[0].physical_address_part1 = p3_addr >> 12;
#elif ___BITS == 64
	p4->pages[0].physical_address = p3_addr >> 12;
#endif

	p3->pages[0].present = 1;
	p3->pages[0].writable = 1;
	size_t p2_addr = memory_get_absolute_address((size_t)p2);
#if ___BITS == 16
	p3->pages[0].physical_address_part1 = p2_addr >> 12;
#elif ___BITS == 64
	p3->pages[0].physical_address = p2_addr >> 12;
#endif

	p2->pages[0].present = 1;
	p2->pages[0].writable = 1;
	p2->pages[0].hugepage = 1;

	page_table_t* t_p3;
	page_table_t* t_p2;

	for(uint32_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
		if(!(SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_ACPI || SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_ACPI_NVS )) {
			continue;
		}
		uint32_t p4idx = MEMORY_PT_GET_P4_INDEX(SYSTEM_INFO->mmap[i].base);
		if(p4idx > 511) {
			return -1;
		}
		if(p4->pages[p4idx].present != 1) {
			t_p3 = memory_simple_kmalloc_aligned(sizeof(page_table_t), 0x1000);
			if(t_p3 == NULL) {
				return -1;
			}
			p4->pages[p4idx].present = 1;
			p4->pages[p4idx].writable = 1;
			p3_addr = memory_get_absolute_address((size_t)p3);
#if ___BITS == 16
			p4->pages[p4idx].physical_address_part1 = p3_addr >> 12;
#elif ___BITS == 64
			p4->pages[p4idx].physical_address = p3_addr >> 12;
#endif
		} else {
#if ___BITS == 16
			t_p3 = (page_table_t*)memory_get_relative_address(p4->pages[p4idx].physical_address_part1 << 12);
#elif ___BITS == 64
			t_p3 = (page_table_t*)((uint64_t)(p4->pages[p4idx].physical_address << 12));
#endif
		}
		uint32_t p3idx = MEMORY_PT_GET_P3_INDEX(SYSTEM_INFO->mmap[i].base);
		if(p3idx > 511) {
			return -1;
		}
		if(t_p3->pages[p3idx].present != 1) {
			t_p2 = memory_simple_kmalloc_aligned(sizeof(page_table_t), 0x1000);
			if(t_p2 == NULL) {
				return -1;
			}
			t_p3->pages[p3idx].present = 1;
			t_p3->pages[p3idx].writable = 1;
			p2_addr = memory_get_absolute_address((size_t)p3);
#if ___BITS == 16
			t_p3->pages[p3idx].physical_address_part1 = p2_addr >> 12;
#elif ___BITS == 64
			t_p3->pages[p3idx].physical_address = p2_addr >> 12;
#endif
		} else {
#if ___BITS == 16
			t_p2 = (page_table_t*)memory_get_relative_address(t_p3->pages[p3idx].physical_address_part1 << 12);
#elif ___BITS == 64
			t_p2 = (page_table_t*)((uint64_t)(t_p3->pages[p3idx].physical_address << 12));
#endif
		}
		uint32_t p2idx = MEMORY_PT_GET_P2_INDEX(SYSTEM_INFO->mmap[i].base);
		if(p2idx > 511) {
			return -1;
		}
		if(t_p2->pages[p2idx].present != 1) {
			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].writable = 1;
			t_p2->pages[p2idx].hugepage = 1;

#if ___BITS == 16
			t_p2->pages[p2idx].physical_address_part1 = ((SYSTEM_INFO->mmap[i].base.part_low >> 12) & 0x000FFE00) | (0XFFF00000 & (SYSTEM_INFO->mmap[i].base.part_high << 20));
			t_p2->pages[p2idx].physical_address_part2 = (SYSTEM_INFO->mmap[i].base.part_high >> 12) & 0xFF;
#elif ___BITS == 64
			t_p2->pages[p2idx].physical_address = (SYSTEM_INFO->mmap[i].base >> 12) & 0xFFFFFFFE00;
#endif

		}
	}
	size_t p4addr_a = memory_get_absolute_address((size_t)p4);
#if ___BITS == 16
	__asm__ __volatile__ ("mov %0, %%cr3\n" : : "r" (p4addr_a));
#elif ___BITS == 64
	__asm__ __volatile__ ("mov %0, %%cr3\n" : : "r" (p4addr_a));
#endif
	return 0;
}
