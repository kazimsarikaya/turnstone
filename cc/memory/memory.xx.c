/**
 * @file memory.xx.c
 * @brief main memory interface and functions implementation
 */
#include <types.h>
#include <memory.h>
#include <memory/paging.h>
#include <cpu.h>
#include <systeminfo.h>
#include <video.h>
#include <cpu/task.h>
#include <cpu/sync.h>
#include <linker.h>
#include <cpu/descriptor.h>

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
		task_t* current_task = task_get_current_task();
		if(current_task != NULL && current_task->heap != NULL) {
			lock_acquire(current_task->heap->lock);
			res = current_task->heap->malloc(current_task->heap, size, align);
			lock_release(current_task->heap->lock);
		}

		if(!res) {
			lock_acquire(memory_heap_default->lock);
			res = memory_heap_default->malloc(memory_heap_default, size, align);
			lock_release(memory_heap_default->lock);
		}

	}else {
		lock_acquire(heap->lock);
		res = heap->malloc(heap, size, align);
		lock_release(heap->lock);
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
		task_t* current_task = task_get_current_task();
		if(current_task != NULL && current_task->heap != NULL) {
			lock_acquire(current_task->heap->lock);
			res = current_task->heap->free(current_task->heap, address);
			lock_release(current_task->heap->lock);
		}

		lock_acquire(memory_heap_default->lock);

		res = memory_heap_default->free(memory_heap_default, address);
		lock_release(memory_heap_default->lock);
	}else {
		lock_acquire(heap->lock);
		res = heap->free(heap, address);
		lock_release(heap->lock);
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

memory_page_table_t* memory_paging_switch_table(const memory_page_table_t* new_table) {
	size_t old_table;
	__asm__ __volatile__ ("mov %%cr3, %0\n"
	                      : "=r" (old_table));
	size_t old_table_r = old_table; // TODO: find virtual address
	if(new_table != NULL) {
		__asm__ __volatile__ ("mov %0, %%cr3\n"
		                      : : "r" (new_table));
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

		if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
			p4->pages[p4idx].writable = 0;
		} else {
			p4->pages[p4idx].writable = 1;
		}

		if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
			p4->pages[p4idx].no_execute = 1;
		} else {
			p4->pages[p4idx].no_execute = 0;
		}

		if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
			p4->pages[p4idx].user_accessible = 1;
		} else {
			p4->pages[p4idx].user_accessible = 0;
		}

		p3_addr = (size_t)t_p3;

		p4->pages[p4idx].physical_address = p3_addr >> 12;

	} else {
		t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4idx].physical_address << 12));
	}

	size_t p3idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

	if(p3idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p3->pages[p3idx].present != 1) {
		if(type & MEMORY_PAGING_PAGE_TYPE_1G) {
			t_p3->pages[p3idx].present = 1;
			t_p3->pages[p3idx].hugepage = 1;

			if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
				t_p3->pages[p3idx].writable = 0;
			} else {
				t_p3->pages[p3idx].writable = 1;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
				t_p3->pages[p3idx].no_execute = 1;
			} else {
				t_p3->pages[p3idx].no_execute = 0;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
				t_p3->pages[p3idx].user_accessible = 1;
			} else {
				t_p3->pages[p3idx].user_accessible = 0;
			}

			t_p3->pages[p3idx].physical_address = (frame_adress >> 12) & 0xFFFFFC0000;

			return 0;
		} else {
			t_p2 = memory_paging_malloc_page_with_heap(heap);

			if(t_p2 == NULL) {
				return -1;
			}

			t_p3->pages[p3idx].present = 1;
			t_p3->pages[p3idx].writable = 1;

			p2_addr = (size_t)t_p2;

			t_p3->pages[p3idx].physical_address = p2_addr >> 12;
		}
	} else {
		if(type & MEMORY_PAGING_PAGE_TYPE_1G) {
			return 0;
		}

		t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3idx].physical_address << 12));
	}

	size_t p2idx = MEMORY_PT_GET_P2_INDEX(virtual_address);

	if(p2idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p2->pages[p2idx].present != 1) {
		if(type & MEMORY_PAGING_PAGE_TYPE_2M) {
			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].hugepage = 1;


			if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
				t_p2->pages[p2idx].writable = 0;
			} else {
				t_p2->pages[p2idx].writable = 1;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
				t_p2->pages[p2idx].no_execute = 1;
			} else {
				t_p2->pages[p2idx].no_execute = 0;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
				t_p2->pages[p2idx].user_accessible = 1;
			} else {
				t_p2->pages[p2idx].user_accessible = 0;
			}

			t_p2->pages[p2idx].physical_address = (frame_adress >> 12) & 0xFFFFFFFE00;

			return 0;
		} else {
			t_p1 = memory_paging_malloc_page_with_heap(heap);

			if(t_p1 == NULL) {
				return -1;
			}

			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].writable = 1;

			p1_addr = (size_t)t_p1;

			t_p2->pages[p2idx].physical_address = p1_addr >> 12;
		}
	} else {
		if(type & MEMORY_PAGING_PAGE_TYPE_2M) {
			return 0;
		}

		t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2idx].physical_address << 12));
	}

	size_t p1idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

	if(p1idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(t_p1->pages[p1idx].present != 1) {
		t_p1->pages[p1idx].present = 1;

		if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
			t_p1->pages[p1idx].writable = 0;
		} else {
			t_p1->pages[p1idx].writable = 1;
		}

		if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
			t_p1->pages[p1idx].no_execute = 1;
		} else {
			t_p1->pages[p1idx].no_execute = 0;
		}

		if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
			t_p1->pages[p1idx].user_accessible = 1;
		} else {
			t_p1->pages[p1idx].user_accessible = 0;
		}

		t_p1->pages[p1idx].physical_address = frame_adress >> 12;
	}

	return 0;
}

memory_page_table_t* memory_paging_build_table_ext(memory_heap_t* heap){
	memory_page_table_t* p4 = memory_paging_malloc_page_with_heap(heap);
	if(p4 == NULL) {
		return NULL;
	}

	program_header_t* kernel_header = (program_header_t*)SYSTEM_INFO->kernel_start;

	for(uint64_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
		uint64_t section_start = kernel_header->section_locations[i].section_start + SYSTEM_INFO->kernel_start;
		uint64_t section_size = kernel_header->section_locations[i].section_size;

		if(i == LINKER_SECTION_TYPE_TEXT) {
			section_size += kernel_header->section_locations[i].section_start;
		}

		if(i == LINKER_SECTION_TYPE_HEAP) {
			extern size_t __kheap_bottom;
			size_t heap_start = (size_t)&__kheap_bottom;
			size_t heap_end = SYSTEM_INFO->kernel_start + SYSTEM_INFO->kernel_4k_frame_count * 0x1000;
			section_size = heap_end - heap_start;
		}

		memory_paging_page_type_t p_type = MEMORY_PAGING_PAGE_TYPE_4K;

		if(section_size % MEMORY_PAGING_PAGE_LENGTH_4K) {
			section_size += MEMORY_PAGING_PAGE_LENGTH_4K - (section_size % MEMORY_PAGING_PAGE_LENGTH_4K);
		}

		printf("section %i start 0x%08x size 0x%08x\n", i, section_start, section_size);

		if(i != LINKER_SECTION_TYPE_TEXT) {
			p_type |= MEMORY_PAGING_PAGE_TYPE_NOEXEC;
		}

		if(i == LINKER_SECTION_TYPE_RODATA) {
			p_type |= MEMORY_PAGING_PAGE_TYPE_READONLY;
		}

		while(section_size) {
			if(memory_paging_add_page_ext(heap, p4, section_start, section_start, p_type) != 0) {
				return NULL;
			}

			section_start += MEMORY_PAGING_PAGE_LENGTH_4K;
			section_size -= MEMORY_PAGING_PAGE_LENGTH_4K;
		}

	}

	if(memory_paging_add_page_ext(heap, p4, IDT_BASE_ADDRESS, IDT_BASE_ADDRESS, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return NULL;
	}

	uint64_t vfb_addr = (uint64_t)SYSTEM_INFO->frame_buffer->base_address;
	uint64_t vfb_size = SYSTEM_INFO->frame_buffer->buffer_size;

	uint64_t vfb_page_size = MEMORY_PAGING_PAGE_LENGTH_2M;

	while(vfb_size > 0) {
		if(vfb_size >= MEMORY_PAGING_PAGE_LENGTH_2M) {
			if(memory_paging_add_page_ext(heap, p4, vfb_addr, vfb_addr, MEMORY_PAGING_PAGE_TYPE_2M | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
				return NULL;
			}
		} else {
			vfb_page_size = MEMORY_PAGING_PAGE_LENGTH_4K;
			if(memory_paging_add_page_ext(heap, p4, vfb_addr, vfb_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
				return NULL;
			}
		}

		vfb_size -= vfb_page_size;
		vfb_addr += vfb_page_size;
	}


	return p4;
}

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

			new_p4->pages[p4_idx].physical_address = ((size_t)new_p3 >> 12) & 0xFFFFFFFFFF;
			memory_page_table_t* t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));

			for(size_t p3_idx = 0; p3_idx < MEMORY_PAGING_INDEX_COUNT; p3_idx++) {
				if(t_p3->pages[p3_idx].present == 1) {
					memory_memcopy(&t_p3->pages[p3_idx], &new_p3->pages[p3_idx], sizeof(memory_page_entry_t));

					if(t_p3->pages[p3_idx].hugepage != 1) {
						memory_page_table_t* new_p2 = memory_paging_malloc_page_with_heap(heap);

						if(new_p2 == NULL) {
							goto cleanup;
						}

						new_p3->pages[p3_idx].physical_address = ((size_t)new_p2 >> 12) & 0xFFFFFFFFFF;
						memory_page_table_t* t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));

						for(size_t p2_idx = 0; p2_idx < MEMORY_PAGING_INDEX_COUNT; p2_idx++) {
							if(t_p2->pages[p2_idx].present == 1) {
								memory_memcopy(&t_p2->pages[p2_idx], &new_p2->pages[p2_idx], sizeof(memory_page_entry_t));

								if(t_p2->pages[p2_idx].hugepage != 1) {
									memory_page_table_t* new_p1 = memory_paging_malloc_page_with_heap(heap);

									if(new_p1 == NULL) {
										goto cleanup;
									}

									new_p2->pages[p2_idx].physical_address = ((size_t)new_p1 >> 12) & 0xFFFFFFFFFF;
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
