/**
 * @file memory.xx.c
 * @brief main memory interface and functions implementation
 */
#include <types.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/mmap.h>
#include <cpu.h>
#include <systeminfo.h>

/*! default heap variable */
memory_heap_t* memory_heap_default = NULL;

memory_heap_t* memory_set_default_heap(memory_heap_t* heap) {
	memory_heap_t* res = memory_heap_default;
	memory_heap_default = heap;
	return res;
}

void* memory_malloc_ext(struct memory_heap* heap, size_t size, size_t align){
	if(heap == NULL) {
		return memory_heap_default->malloc(memory_heap_default, size, align);
	}else {
		return heap->malloc(heap, size, align);
	}
}

uint8_t memory_free_ext(struct memory_heap* heap, void* address){
	if(heap == NULL) {
		return memory_heap_default->free(memory_heap_default, address);
	}else {
		return heap->free(heap, address);
	}
}

uint8_t memory_memset(void* address, uint8_t value, size_t size){
	for(size_t i = 0; i < size; i++) {
		((uint8_t*)address)[i] = value;
	}
	return 0;
}

uint8_t memory_memcopy(void* source, void* destination, size_t length){
	uint8_t* src = (uint8_t*)source;
	uint8_t* dst = (uint8_t*)destination;
	for(size_t i = 0; i < length; i++) {
		dst[i] = src[i];
	}
	return 0;
}

int8_t memory_memcompare(void* mem1, void* mem2, size_t length) {
	uint8_t* mem1_t = (uint8_t*)mem1;
	uint8_t* mem2_t = (uint8_t*)mem2;
	for(size_t i = 0; i < length; i++) {
		if(mem1_t[i] < mem2_t[i]) {
			return -1;
		} else if(mem1_t[i] == mem2_t[i]) {
			continue;
		} else if(mem1_t[i] > mem2_t[i]) {
			return 1;
		}
	}
	return 0;
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

size_t memory_get_relative_address(size_t aaddr) {
#if ___BITS == 16
	size_t ds = cpu_read_data_segment();
	ds <<= 4;
	ds = aaddr - ds;
	return ds;
#endif
	return aaddr;
}

memory_page_table_t* memory_paging_switch_table(const memory_page_table_t* new_table) {
	size_t old_table;
	size_t new_table_a = memory_get_absolute_address((size_t)new_table);
	__asm__ __volatile__ ("mov %%cr3, %0\n"
	                      : "=r" (old_table));
	size_t old_table_r = memory_get_relative_address(old_table);
	if(new_table != NULL) {
		__asm__ __volatile__ ("mov %0, %%cr3\n"
		                      : : "r" (new_table_a));
	}
	return (memory_page_table_t*)old_table_r;
}


uint8_t memory_paging_add_page_ext(memory_heap_t* heap, memory_page_table_t* p4,
                                   uint64_t virtual_address, uint64_t frame_adress, memory_paging_page_type_t type){
	// TODO: implement page type
	memory_page_table_t* t_p3;
	memory_page_table_t* t_p2;
	size_t p3_addr, p2_addr;

	uint32_t p4idx = MEMORY_PT_GET_P4_INDEX(virtual_address);
	if(p4idx > 511) {
		return -1;
	}
	if(p4->pages[p4idx].present != 1) {
		t_p3 = memory_malloc_ext(heap, sizeof(memory_page_table_t), 0x1000);
		if(t_p3 == NULL) {
			return NULL;
		}
		p4->pages[p4idx].present = 1;
		p4->pages[p4idx].writable = 1;
		p3_addr = memory_get_absolute_address((size_t)t_p3);
#if ___BITS == 16
		p4->pages[p4idx].physical_address_part1 = p3_addr >> 12;
#elif ___BITS == 64
		p4->pages[p4idx].physical_address = p3_addr >> 12;
#endif
	} else {
#if ___BITS == 16
		t_p3 = (memory_page_table_t*)memory_get_relative_address(p4->pages[p4idx].physical_address_part1 << 12);
#elif ___BITS == 64
		t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4idx].physical_address << 12));
#endif
	}
	uint32_t p3idx = MEMORY_PT_GET_P3_INDEX(virtual_address);
	if(p3idx > 511) {
		return NULL;
	}
	if(t_p3->pages[p3idx].present != 1) {
		t_p2 = memory_malloc_ext(heap, sizeof(memory_page_table_t), 0x1000);
		if(t_p2 == NULL) {
			return -1;
		}
		t_p3->pages[p3idx].present = 1;
		t_p3->pages[p3idx].writable = 1;
		p2_addr = memory_get_absolute_address((size_t)t_p2);
#if ___BITS == 16
		t_p3->pages[p3idx].physical_address_part1 = p2_addr >> 12;
#elif ___BITS == 64
		t_p3->pages[p3idx].physical_address = p2_addr >> 12;
#endif
	} else {
#if ___BITS == 16
		t_p2 = (memory_page_table_t*)memory_get_relative_address(t_p3->pages[p3idx].physical_address_part1 << 12);
#elif ___BITS == 64
		t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3idx].physical_address << 12));
#endif
	}
	uint32_t p2idx = MEMORY_PT_GET_P2_INDEX(virtual_address);
	if(p2idx > 511) {
		return -1;
	}
	if(t_p2->pages[p2idx].present != 1) {
		t_p2->pages[p2idx].present = 1;
		t_p2->pages[p2idx].writable = 1;
		t_p2->pages[p2idx].hugepage = 1;

#if ___BITS == 16
		t_p2->pages[p2idx].physical_address_part1 = ((frame_adress.part_low >> 12) & 0x000FFE00) | (0XFFF00000 & (frame_adress.part_high << 20));
		t_p2->pages[p2idx].physical_address_part2 = (frame_adress.part_high >> 12) & 0xFF;
#elif ___BITS == 64
		t_p2->pages[p2idx].physical_address = (frame_adress >> 12) & 0xFFFFFFFE00;
#endif
	}
	return 0;
}

memory_page_table_t* memory_paging_build_table_ext(memory_heap_t* heap){
	memory_page_table_t* p4 = memory_malloc_ext(heap, sizeof(memory_page_table_t), 0x1000);
	if(p4 == NULL) {
		return NULL;
	}
#if ___BITS == 16
	uint64_t base_addr_zero = {0, 0};
#elif ___BITS == 64
	uint64_t base_addr_zero = 0;
#endif
	if(memory_paging_add_page_ext(heap, p4, base_addr_zero, base_addr_zero, MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
		return NULL;
	}

	for(uint32_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
		if(SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_USABLE) {
			continue;
		}
		// TODO: check memory area length if greater then 2M
		if(memory_paging_add_page_ext(heap, p4, SYSTEM_INFO->mmap[i].base, SYSTEM_INFO->mmap[i].base, MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
			return NULL;
		}
	}
	return p4;
}



size_t memory_detect_map(memory_map_t** mmap) {
	*mmap = memory_malloc(sizeof(memory_map_t) * MEMORY_MMAP_MAX_ENTRY_COUNT);
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

	memory_map_t* mmap_r = memory_malloc(sizeof(memory_map_t) * entries);
	memory_memcopy(*mmap, mmap_r, sizeof(memory_map_t) * entries);
	memory_free(*mmap);
	*mmap = mmap_r;

	return entries;
}
