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
#include <video.h>
#if ___BITS == 64
#include <cpu/task.h>
#include <cpu/sync.h>
#endif

/*! default heap variable */
memory_heap_t* memory_heap_default = NULL;

memory_heap_t* memory_set_default_heap(memory_heap_t* heap) {
	memory_heap_t* res = memory_heap_default;
	memory_heap_default = heap;
	return res;
}

void* memory_malloc_ext(memory_heap_t* heap, size_t size, size_t align){
	if(size % 8) {
		size = size + (8 - (size % 8));
	}

	void* res = NULL;

	if(heap == NULL) {
#if ___BITS == 64
		task_t* current_task = task_get_current_task();
		if(current_task != NULL && current_task->heap != NULL) {
			lock_acquire(current_task->heap->lock);
			res = current_task->heap->malloc(current_task->heap, size, align);
			lock_release(current_task->heap->lock);
		}
#endif

		if(!res) {
#if ___BITS == 64
			lock_acquire(memory_heap_default->lock);
#endif
			res = memory_heap_default->malloc(memory_heap_default, size, align);
#if ___BITS == 64
			lock_release(memory_heap_default->lock);
#endif
		}

	}else {
#if ___BITS == 64
		lock_acquire(heap->lock);
#endif
		res = heap->malloc(heap, size, align);
#if ___BITS == 64
		lock_release(heap->lock);
#endif
	}

	if(res != NULL) {
		if(align && ((size_t)res % align)) {
			memory_free_ext(heap, res); // recover
			res = NULL;
		}
	}

	return res;
}

int8_t memory_free_ext(memory_heap_t* heap, void* address){
	int8_t res = -1;

	if(heap == NULL) {
#if ___BITS == 64
		task_t* current_task = task_get_current_task();
		if(current_task != NULL && current_task->heap != NULL) {
			lock_acquire(current_task->heap->lock);
			res = current_task->heap->free(current_task->heap, address);
			lock_release(current_task->heap->lock);
		}

		lock_acquire(memory_heap_default->lock);
#endif

		res = memory_heap_default->free(memory_heap_default, address);
#if ___BITS == 64
		lock_release(memory_heap_default->lock);
#endif
	}else {
#if ___BITS == 64
		lock_acquire(heap->lock);
#endif
		res = heap->free(heap, address);
#if ___BITS == 64
		lock_release(heap->lock);
#endif
	}

	return res;
}

int8_t memory_memset(void* address, uint8_t value, size_t size){
	uint8_t* t_addr = (uint8_t*)address;

	size_t max_regsize = sizeof(size_t);

	if(size <= (max_regsize * 32)) {
		for(size_t i = 0; i < size; i++) {
			*t_addr++ = value;
		}
		return 0;
	}

	size_t start = (size_t)address;

	size_t rem = start % max_regsize;
	if(rem != 0) {
		rem = max_regsize - rem;

		for(size_t i = 0; i < rem; i++) {
			*t_addr++ = value;
			size--;
		}
	}

	size_t* st_addr = (size_t*)t_addr;

	size_t rep = size / max_regsize;

	size_t pad = 0;

	if(value) {
		for(uint8_t i = 0; i < max_regsize; i++) {
			pad |= ((size_t)value) << (4 * i);
		}
	}

	for(size_t i = 0; i < rep; i++) {
		*st_addr++ = pad;
	}

	rem = size % max_regsize;

	if(rem > 0) {
		t_addr = (uint8_t*)st_addr;

		for(size_t i = 0; i < rem; i++) {
			*t_addr++ = value;
		}
	}

	return 0;
}

int8_t memory_memclean(void* address, size_t size){
	return memory_memset(address, 0, size);
}

int8_t memory_memcopy(void* source, void* destination, size_t size){
	uint8_t* s_addr = (uint8_t*)source;
	uint8_t* t_addr = (uint8_t*)destination;

	size_t max_regsize = sizeof(size_t);

	if(size <= (max_regsize * 2)) {
		for(size_t i = 0; i < size; i++) {
			*t_addr++ = *s_addr++;
		}
		return 0;
	}

	size_t start = (size_t)source;

	size_t rem = start % max_regsize;

	if(rem != 0) {
		rem = max_regsize - rem;

		for(size_t i = 0; i < rem; i++) {
			*t_addr++ = *s_addr++;
			size--;
		}
	}

	size_t* st_addr = (size_t*)t_addr;
	size_t* ss_addr = (size_t*)s_addr;

	size_t rep = size / max_regsize;

	for(size_t i = 0; i < rep; i++) {
		*st_addr++ = *ss_addr++;
	}

	rem = size % max_regsize;

	if(rem > 0) {
		t_addr = (uint8_t*)st_addr;
		s_addr = (uint8_t*)ss_addr;

		for(size_t i = 0; i < rem; i++) {
			*t_addr++ = *s_addr++;
		}
	}

	return 0;
}

int8_t memory_memcompare(void* mem1, void* mem2, size_t size) {
	uint8_t* mem1_t = (uint8_t*)mem1;
	uint8_t* mem2_t = (uint8_t*)mem2;
	for(size_t i = 0; i < size; i++) {
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

int8_t memory_paging_add_page_ext(memory_heap_t* heap, memory_page_table_t* p4,
                                  uint64_t virtual_address, uint64_t frame_adress,
                                  memory_paging_page_type_t type) {
	// TODO: implement page type
	memory_page_table_t* t_p3;
	memory_page_table_t* t_p2;
	memory_page_table_t* t_p1;
	size_t p3_addr, p2_addr, p1_addr;

	if(p4 == NULL) {
		p4 = memory_paging_switch_table(NULL);
	}

	size_t p4idx = MEMORY_PT_GET_P4_INDEX(virtual_address);
	if(p4idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(p4->pages[p4idx].present != 1) {
		t_p3 = memory_paging_malloc_page_with_heap(heap);

		if(t_p3 == NULL) {
			return -1;
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

	size_t p3idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

	if(p3idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p3->pages[p3idx].present != 1) {
		if(type == MEMORY_PAGING_PAGE_TYPE_1G) {
			t_p3->pages[p3idx].present = 1;
			t_p3->pages[p3idx].writable = 1;
			t_p3->pages[p3idx].hugepage = 1;

#if ___BITS == 16
			t_p3->pages[p3idx].physical_address_part1 = ((frame_adress.part_low >> 12) & 0x000C0000) | (0XFFF00000 & (frame_adress.part_high << 20));
			t_p3->pages[p3idx].physical_address_part2 = (frame_adress.part_high >> 12) & 0xFF;
#elif ___BITS == 64
			t_p3->pages[p3idx].physical_address = (frame_adress >> 12) & 0xFFFFFC0000;
#endif

			return 0;
		} else {
			t_p2 = memory_paging_malloc_page_with_heap(heap);

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
		}
	} else {
		if(type == MEMORY_PAGING_PAGE_TYPE_1G) {
			return 0;
		}

#if ___BITS == 16
		t_p2 = (memory_page_table_t*)memory_get_relative_address(t_p3->pages[p3idx].physical_address_part1 << 12);
#elif ___BITS == 64
		t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3idx].physical_address << 12));
#endif
	}

	size_t p2idx = MEMORY_PT_GET_P2_INDEX(virtual_address);

	if(p2idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p2->pages[p2idx].present != 1) {
		if(type == MEMORY_PAGING_PAGE_TYPE_2M) {
			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].writable = 1;
			t_p2->pages[p2idx].hugepage = 1;

#if ___BITS == 16
			t_p2->pages[p2idx].physical_address_part1 = ((frame_adress.part_low >> 12) & 0x000FFE00) | (0XFFF00000 & (frame_adress.part_high << 20));
			t_p2->pages[p2idx].physical_address_part2 = (frame_adress.part_high >> 12) & 0xFF;
#elif ___BITS == 64
			t_p2->pages[p2idx].physical_address = (frame_adress >> 12) & 0xFFFFFFFE00;
#endif

			return 0;
		} else {
			t_p1 = memory_paging_malloc_page_with_heap(heap);

			if(t_p1 == NULL) {
				return -1;
			}

			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].writable = 1;
			p1_addr = memory_get_absolute_address((size_t)t_p1);

#if ___BITS == 16
			t_p2->pages[p2idx].physical_address_part1 = p1_addr >> 12;
#elif ___BITS == 64
			t_p2->pages[p2idx].physical_address = p1_addr >> 12;
#endif
		}
	} else {
		if(type == MEMORY_PAGING_PAGE_TYPE_2M) {
			return 0;
		}

#if ___BITS == 16
		t_p1 = (memory_page_table_t*)memory_get_relative_address(t_p2->pages[p2idx].physical_address_part1 << 12);
#elif ___BITS == 64
		t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2idx].physical_address << 12));
#endif
	}

	size_t p1idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

	if(p1idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p1->pages[p1idx].present != 1) {
		t_p1->pages[p1idx].present = 1;
		t_p1->pages[p1idx].writable = 1;

#if ___BITS == 16
		t_p1->pages[p1idx].physical_address_part1 = ((frame_adress.part_low >> 12) & 0x000FFE00) | (0XFFF00000 & (frame_adress.part_high << 20));
		t_p1->pages[p1idx].physical_address_part2 = (frame_adress.part_high >> 12) & 0xFF;
#elif ___BITS == 64
		t_p1->pages[p1idx].physical_address = frame_adress >> 12;
#endif
	}

	return 0;
}

memory_page_table_t* memory_paging_build_table_ext(memory_heap_t* heap){
	memory_page_table_t* p4 = memory_paging_malloc_page_with_heap(heap);
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

/* We don't need all areas. it should be created mannually
   for(uint32_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
    if(SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_USABLE) {
      continue;
    }
    // TODO: check memory area length if greater then 2M
    if(memory_paging_add_page_ext(heap, p4, SYSTEM_INFO->mmap[i].base, SYSTEM_INFO->mmap[i].base, MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
      return NULL;
    }
   }
 */
	return p4;
}

#if ___BITS == 64

int8_t memory_paging_delete_page_ext_with_heap(memory_heap_t* heap, memory_page_table_t* p4, uint64_t virtual_address, uint64_t* frame_adress){
	if(p4 == NULL) {
		p4 = memory_paging_switch_table(NULL);
	}

	memory_page_table_t* t_p3;
	memory_page_table_t* t_p2;
	memory_page_table_t* t_p1;

	int8_t p1_used = 0, p2_used = 0, p3_used = 0;

	size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);
	if(p4_idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}
	if(p4->pages[p4_idx].present == 0) {
		return -1;
	}

	t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
	size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);
	if(p3_idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p3->pages[p3_idx].present == 0) {
		return -1;
	} else {
		if(t_p3->pages[p3_idx].hugepage == 1) {
			*frame_adress = t_p3->pages[p3_idx].physical_address << 12;
			memory_memclean(&t_p3->pages[p3_idx], sizeof(memory_page_entry_t));
		} else {
			t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
			size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);
			if(p2_idx >= MEMORY_PAGING_INDEX_COUNT) {
				return -1;
			}
			if(t_p2->pages[p2_idx].present == 0) {
				return -1;
			}
			if(t_p2->pages[p2_idx].hugepage == 1) {
				*frame_adress = t_p2->pages[p2_idx].physical_address << 12;
				memory_memclean(&t_p2->pages[p2_idx], sizeof(memory_page_entry_t));
			} else {
				t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
				size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);
				if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
					return -1;
				}
				if(t_p1->pages[p1_idx].present == 0) {
					return -1;
				}
				*frame_adress = t_p1->pages[p1_idx].physical_address << 12;
				memory_memclean(&t_p1->pages[p1_idx], sizeof(memory_page_entry_t));
				for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
					if(t_p1->pages[i].present == 1) {
						p1_used = 1;
						break;
					}
				}
				if(p1_used == 0) {
					memory_memclean(&t_p2->pages[p2_idx], sizeof(memory_page_entry_t));
					memory_free_ext(heap, t_p1);
				}
			}
			for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
				if(t_p2->pages[i].present == 1) {
					p2_used = 1;
					break;
				}
			}
			if(p2_used == 0) {
				memory_memclean(&t_p3->pages[p3_idx], sizeof(memory_page_entry_t));
				memory_free_ext(heap, t_p2);
			}
		}
		for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
			if(t_p3->pages[i].present == 1) {
				p3_used = 1;
				break;
			}
		}
		if(p3_used == 0) {
			memory_memclean(&p4->pages[p4_idx], sizeof(memory_page_entry_t));
			memory_free_ext(heap, t_p3);
		}
	}

	return 0;
}

memory_page_table_t* memory_paging_clone_pagetable_ext(memory_heap_t* heap, memory_page_table_t* p4){
	if(p4 == NULL) {
		p4 = memory_paging_get_table();
	}

	printf("KERNEL: Info old page table is at 0x%lx\n", p4);

	memory_page_table_t* new_p4 = memory_paging_malloc_page_with_heap(heap);

	if(new_p4 == NULL) {
		printf("KERNEL: Fatal cannot create new page table\n");
		return NULL;
	}

	printf("KERNEL: Info new page table is at 0x%lx\n", new_p4);

	for(size_t p4_idx = 0; p4_idx < MEMORY_PAGING_INDEX_COUNT; p4_idx++) {
		if(p4->pages[p4_idx].present == 1) {
			memory_memcopy(&p4->pages[p4_idx], &new_p4->pages[p4_idx], sizeof(memory_page_entry_t));

			memory_page_table_t* new_p3 = memory_paging_malloc_page_with_heap(heap);

			if(new_p3 == NULL) {
				goto cleanup;
			}

			new_p4->pages[p4_idx].physical_address = ( memory_get_absolute_address((size_t)new_p3) >> 12) & 0xFFFFFFFFFF;
			memory_page_table_t* t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));

			for(size_t p3_idx = 0; p3_idx < MEMORY_PAGING_INDEX_COUNT; p3_idx++) {
				if(t_p3->pages[p3_idx].present == 1) {
					memory_memcopy(&t_p3->pages[p3_idx], &new_p3->pages[p3_idx], sizeof(memory_page_entry_t));

					if(t_p3->pages[p3_idx].hugepage != 1) {
						memory_page_table_t* new_p2 = memory_paging_malloc_page_with_heap(heap);

						if(new_p2 == NULL) {
							goto cleanup;
						}

						new_p3->pages[p3_idx].physical_address = (memory_get_absolute_address((size_t)new_p2) >> 12) & 0xFFFFFFFFFF;
						memory_page_table_t* t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));

						for(size_t p2_idx = 0; p2_idx < MEMORY_PAGING_INDEX_COUNT; p2_idx++) {
							if(t_p2->pages[p2_idx].present == 1) {
								memory_memcopy(&t_p2->pages[p2_idx], &new_p2->pages[p2_idx], sizeof(memory_page_entry_t));

								if(t_p2->pages[p2_idx].hugepage != 1) {
									memory_page_table_t* new_p1 = memory_paging_malloc_page_with_heap(heap);

									if(new_p1 == NULL) {
										goto cleanup;
									}

									new_p2->pages[p2_idx].physical_address = (memory_get_absolute_address((size_t)new_p1) >> 12) & 0xFFFFFFFFFF;
									memory_page_table_t* t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
									memory_memcopy(t_p1, new_p1, sizeof(memory_page_table_t));
								}
							}
						}
					}
				}
			}
		}
	}

	return new_p4;

cleanup:
	printf("KERNEL: Fatal error at cloning page table\n");
	memory_paging_destroy_pagetable_ext(heap, new_p4);

	return NULL;
}

int8_t memory_paging_destroy_pagetable_ext(memory_heap_t* heap, memory_page_table_t* p4){
#if ___TESTMODE != 1
	memory_page_table_t* cur_p4 = memory_paging_switch_table(NULL);
	if(cur_p4 == p4) {
		return -2;
	}
#endif

	for(size_t p4_idx = 0; p4_idx < MEMORY_PAGING_INDEX_COUNT; p4_idx++) {
		if(p4->pages[p4_idx].present == 1) {
			memory_page_table_t* t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));

			if(t_p3 == NULL) {
				continue;
			}

			for(size_t p3_idx = 0; p3_idx < MEMORY_PAGING_INDEX_COUNT; p3_idx++) {
				if(t_p3->pages[p3_idx].present == 1 && t_p3->pages[p3_idx].hugepage != 1) {
					memory_page_table_t* t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));

					if(t_p2 == NULL) {
						continue;
					}

					for(size_t p2_idx = 0; p2_idx < MEMORY_PAGING_INDEX_COUNT; p2_idx++) {
						if(t_p2->pages[p2_idx].present == 1 && t_p2->pages[p2_idx].hugepage != 1) {
							memory_page_table_t* t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
							memory_free_ext(heap, t_p1);
						}
					}

					memory_free_ext(heap, t_p2);
				}
			}

			memory_free_ext(heap, t_p3);
		}
	}

	memory_free_ext(heap, p4);

	return 0;
}

int8_t memory_paging_get_frame_address_ext(memory_page_table_t* p4, uint64_t virtual_address, uint64_t* frame_adress){
	if(p4 == NULL) {
		p4 = memory_paging_switch_table(NULL);
	}
	memory_page_table_t* t_p3;
	memory_page_table_t* t_p2;
	memory_page_table_t* t_p1;

	size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);
	if(p4_idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}
	if(p4->pages[p4_idx].present == 0) {
		return -1;
	}

	t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
	size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);
	if(p3_idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p3->pages[p3_idx].present == 0) {
		return -1;
	} else {
		if(t_p3->pages[p3_idx].hugepage == 1) {
			*frame_adress = t_p3->pages[p3_idx].physical_address << 12;
		} else {
			t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
			size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);
			if(p2_idx >= MEMORY_PAGING_INDEX_COUNT) {
				return -1;
			}
			if(t_p2->pages[p2_idx].present == 0) {
				return -1;
			}
			if(t_p2->pages[p2_idx].hugepage == 1) {
				*frame_adress = t_p2->pages[p2_idx].physical_address << 12;
			} else {
				t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
				size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);
				if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
					return -1;
				}
				if(t_p1->pages[p1_idx].present == 0) {
					return -1;
				}
				*frame_adress = t_p1->pages[p1_idx].physical_address << 12;
			}
		}
	}
	return 0;
}

#endif

size_t memory_detect_map(memory_map_t** mmap) {
	*mmap = memory_malloc(sizeof(memory_map_t) * MEMORY_MMAP_MAX_ENTRY_COUNT);
	memory_map_t* mmap_a = *mmap;
	regext_t contID = 0, signature, bytes;
	size_t entries = 0;
	int16_t err;

	do {
		__asm__ __volatile__ ("int $0x15"
		                      : "=@ccc" (err), "=a" (signature), "=c" (bytes), "=b" (contID)
		                      : "a" (0xE820), "b" (contID), "c" (24), "d" (0x534D4150), "D" (mmap_a));

		if(err) {
			printf("MMAP: Info E820 not supported\n");
			break;
		}

		if(signature != 0x534D4150) {
			return -1;
		}

		mmap_a++;
		entries++;

	} while(contID != 0 && entries < MEMORY_MMAP_MAX_ENTRY_COUNT);

	if(err) {
		return -1; // TODO: implement other methods
	}

	memory_map_t* mmap_r = memory_malloc(sizeof(memory_map_t) * entries);
	memory_memcopy(*mmap, mmap_r, sizeof(memory_map_t) * entries);
	memory_free(*mmap);
	*mmap = mmap_r;

	return entries;
}
