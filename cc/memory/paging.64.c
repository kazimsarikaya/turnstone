#include <types.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <cpu.h>
#include <systeminfo.h>
#include <video.h>
#include <linker.h>
#include <cpu/descriptor.h>

#define MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT 64

uint64_t MEMORY_PAGING_INTERNAL_FRAMES_1_START = 0;
uint64_t MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT = 0;
uint64_t MEMORY_PAGING_INTERNAL_FRAMES_2_START = 0;
uint64_t MEMORY_PAGING_INTERNAL_FRAMES_2_COUNT = 0;

uint64_t memory_paging_get_internal_frame();

uint64_t memory_paging_get_internal_frame() {
	if(MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT == 0) {
		MEMORY_PAGING_INTERNAL_FRAMES_1_START = MEMORY_PAGING_INTERNAL_FRAMES_2_START;
		MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT = MEMORY_PAGING_INTERNAL_FRAMES_2_COUNT;
		MEMORY_PAGING_INTERNAL_FRAMES_2_START = 0;
		MEMORY_PAGING_INTERNAL_FRAMES_2_COUNT = 0;
	}

	if(MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT < (MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT >> 1)) {
		PRINTLOG("PAGING", "DEBUG", "Second internal page frame cache needs refilling", 0);
		frame_t* internal_frms;

		if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
		                                                   MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT,
		                                                   FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
		                                                   &internal_frms, NULL) != 0) {
			PRINTLOG("PAGING", "PANIC", "cannot allocate internal paging frames. Halting...", 0);
			cpu_hlt();
		}

		MEMORY_PAGING_INTERNAL_FRAMES_2_COUNT = MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT;
		MEMORY_PAGING_INTERNAL_FRAMES_2_START = internal_frms->frame_address;

		for(uint64_t i = 0; i < MEMORY_PAGING_INTERNAL_FRAMES_2_COUNT; i++) {
			if(memory_paging_add_page_ext(NULL, NULL,
			                              MEMORY_PAGING_INTERNAL_FRAMES_2_START + i * MEMORY_PAGING_PAGE_SIZE,
			                              MEMORY_PAGING_INTERNAL_FRAMES_2_START + i * MEMORY_PAGING_PAGE_SIZE,
			                              MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
				PRINTLOG("PAGING", "PANIC", "cannot map internal paging frames. Halting...", 0);
				cpu_hlt();
			}
		}

		PRINTLOG("PAGING", "DEBUG", "Second internal page frame cache refilled", 0);
	}

	uint64_t res = MEMORY_PAGING_INTERNAL_FRAMES_1_START;
	MEMORY_PAGING_INTERNAL_FRAMES_1_START += MEMORY_PAGING_PAGE_SIZE;
	MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT--;

	PRINTLOG("PAGING", "DEBUG", "Internal page frame returns frame 0x%08x", res);

	return res;
}

memory_page_table_t* memory_paging_switch_table(const memory_page_table_t* new_table) {
	size_t old_table;
	__asm__ __volatile__ ("mov %%cr3, %0\n"
	                      : "=r" (old_table));
	size_t old_table_r = old_table; // TODO: find virtual address

	if(new_table != NULL) {
		__asm__ __volatile__ ("mov %0, %%cr3\n" : : "r" (new_table));
	}
	return (memory_page_table_t*)old_table_r;
}

int8_t memory_paging_add_page_ext(memory_heap_t* heap, memory_page_table_t* p4,
                                  uint64_t virtual_address, uint64_t frame_address,
                                  memory_paging_page_type_t type) {
	// TODO: implement page type
	memory_page_table_t* t_p3;
	memory_page_table_t* t_p2;
	memory_page_table_t* t_p1;
	size_t p3_addr, p2_addr, p1_addr;

	boolean_t curr_p4_diffrent_target_p4 = 0;


	memory_page_table_t* curr = memory_paging_switch_table(NULL);

	if(p4 == NULL) {
		p4 = curr;
	} else {
		if(p4 != curr) {
			curr_p4_diffrent_target_p4 = 1;
		}
	}

	size_t p4idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

	if(p4idx >= MEMORY_PAGING_INDEX_COUNT) {
		return -1;
	}

	if(p4->pages[p4idx].present != 1) {

		if(type & MEMORY_PAGING_PAGE_TYPE_INTERNAL) {
			p3_addr = memory_paging_get_internal_frame();
		} else {
			frame_t* t_p3_frm;

			frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED;

			if(type & MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) {
				fa_type |= FRAME_ALLOCATION_TYPE_OLD_RESERVED;
			}

			if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, 1, fa_type, &t_p3_frm, NULL) != 0) {
				KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, t_p3_frm);

				return -1;
			}

			p3_addr = t_p3_frm->frame_address;

			if(curr_p4_diffrent_target_p4 && memory_paging_add_page_ext(heap, NULL, p3_addr, p3_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL | MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) != 0) {
				return -1;
			}

			if(memory_paging_add_page_ext(heap, p4, p3_addr, p3_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL) != 0) {
				return -1;
			}
		}

		t_p3 = (memory_page_table_t*)p3_addr;
		memory_memclean(t_p3, FRAME_SIZE);

		p4->pages[p4idx].present = 1;
		p4->pages[p4idx].writable = 1;
		p4->pages[p4idx].physical_address = p3_addr >> 12;

	}
	else {
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

			t_p3->pages[p3idx].physical_address = (frame_address >> 12) & 0xFFFFFC0000;

			return 0;
		} else {

			if(type & MEMORY_PAGING_PAGE_TYPE_INTERNAL) {
				p2_addr = memory_paging_get_internal_frame();
			} else {
				frame_t* t_p2_frm;

				frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED;

				if(type & MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) {
					fa_type |= FRAME_ALLOCATION_TYPE_OLD_RESERVED;
				}

				if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, 1, fa_type, &t_p2_frm, NULL) != 0) {
					KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, t_p2_frm);

					return -1;
				}

				p2_addr = t_p2_frm->frame_address;

				if(curr_p4_diffrent_target_p4 && memory_paging_add_page_ext(heap, NULL, p2_addr, p2_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL | MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) != 0) {
					return -1;
				}

				if(memory_paging_add_page_ext(heap, p4, p2_addr, p2_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL) != 0) {
					return -1;
				}
			}

			t_p2 = (memory_page_table_t*)p2_addr;
			memory_memclean(t_p2, FRAME_SIZE);

			t_p3->pages[p3idx].present = 1;
			t_p3->pages[p3idx].writable = 1;
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

			t_p2->pages[p2idx].physical_address = (frame_address >> 12) & 0xFFFFFFFE00;

			return 0;
		} else {

			if(type & MEMORY_PAGING_PAGE_TYPE_INTERNAL) {
				p1_addr = memory_paging_get_internal_frame();
			} else {
				frame_t* t_p1_frm;

				frame_allocation_type_t fa_type = FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED;

				if(type & MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) {
					fa_type |= FRAME_ALLOCATION_TYPE_OLD_RESERVED;
				}

				if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, 1, fa_type, &t_p1_frm, NULL) != 0) {
					KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, t_p1_frm);

					return -1;
				}

				p1_addr = t_p1_frm->frame_address;

				if(curr_p4_diffrent_target_p4 && memory_paging_add_page_ext(heap, NULL, p1_addr, p1_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL | MEMORY_PAGING_PAGE_TYPE_WILL_DELETED) != 0) {
					return -1;
				}

				if(memory_paging_add_page_ext(heap, p4, p1_addr, p1_addr, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL) != 0) {
					return -1;
				}
			}

			t_p1 = (memory_page_table_t*)p1_addr;
			memory_memclean(t_p1, FRAME_SIZE);

			t_p2->pages[p2idx].present = 1;
			t_p2->pages[p2idx].writable = 1;
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

		t_p1->pages[p1idx].physical_address = frame_address >> 12;
	}

	return 0;
}

memory_page_table_t* memory_paging_build_table_ext(memory_heap_t* heap){
	frame_t* t_p4_frm;

	if(KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR,
	                                                   MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT,
	                                                   FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
	                                                   &t_p4_frm, NULL) != 0) {
		PRINTLOG("PAGING", "ERROR", "cannot allocate frames for p4", 0);

		return NULL;
	}

	for(uint64_t i = 0; i < MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT; i++) {
		if(memory_paging_add_page_ext(NULL, NULL,
		                              t_p4_frm->frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K,
		                              t_p4_frm->frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K,
		                              MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
			PRINTLOG("PAGING", "ERROR", "cannot allocate internal frames for page table build", 0);

			return NULL;
		}
	}

	memory_page_table_t* p4 = (memory_page_table_t*)t_p4_frm->frame_address;

	MEMORY_PAGING_INTERNAL_FRAMES_1_START = t_p4_frm->frame_address + MEMORY_PAGING_PAGE_LENGTH_4K;
	MEMORY_PAGING_INTERNAL_FRAMES_1_COUNT = MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT - 1;

	PRINTLOG("PAGING", "DEBUG", "frame pool start 0x%016lx", MEMORY_PAGING_INTERNAL_FRAMES_1_START);

	if(memory_paging_add_page_ext(heap, p4, (uint64_t)p4, (uint64_t)p4, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL) != 0) {
		PRINTLOG("PAGING", "ERROR", "cannot build p4's itself", 0);

		return NULL;
	}

	for(uint64_t i = 0; i < MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT; i++) {
		if(memory_paging_add_page_ext(NULL, p4,
		                              t_p4_frm->frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K,
		                              t_p4_frm->frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K,
		                              MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_INTERNAL) != 0) {
			PRINTLOG("PAGING", "ERROR", "cannot allocate internal frames for page table build", 0);

			return NULL;
		}
	}


	program_header_t* kernel_header = (program_header_t*)SYSTEM_INFO->kernel_start;

	uint64_t kernel_offset = 0;
	uint64_t kernel_physical_offset = SYSTEM_INFO->kernel_start;

	if(SYSTEM_INFO->remapped == 0) {
		kernel_offset = SYSTEM_INFO->kernel_start;
	}

	for(uint64_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
		uint64_t section_start = kernel_header->section_locations[i].section_start + kernel_offset;
		uint64_t section_size = kernel_header->section_locations[i].section_size;

		section_start -= (section_start % FRAME_SIZE);

		if(SYSTEM_INFO->remapped == 0 && i == LINKER_SECTION_TYPE_TEXT) {
			section_size += 0x100; // add header to text section size
			kernel_header->section_locations[i].section_pyhsical_start = 0;
			kernel_header->section_locations[i].section_start = section_start;
			kernel_header->section_locations[i].section_size = section_size;
		}

		if(i == LINKER_SECTION_TYPE_HEAP && kernel_header->section_locations[i].section_size == 0) {
			extern size_t __kheap_bottom;
			size_t heap_start = (size_t)&__kheap_bottom;
			size_t heap_end = SYSTEM_INFO->kernel_start + SYSTEM_INFO->kernel_4k_frame_count * 0x1000;
			section_size = heap_end - heap_start;
			kernel_header->section_locations[i].section_start = heap_start - kernel_offset;
			kernel_header->section_locations[i].section_size = section_size;
		}

		memory_paging_page_type_t p_type = MEMORY_PAGING_PAGE_TYPE_4K;

		if(section_size % MEMORY_PAGING_PAGE_LENGTH_4K) {
			section_size += MEMORY_PAGING_PAGE_LENGTH_4K - (section_size % MEMORY_PAGING_PAGE_LENGTH_4K);
		}

		uint64_t fa_addr = kernel_header->section_locations[i].section_pyhsical_start + kernel_physical_offset;

		if(fa_addr % FRAME_SIZE) {
			fa_addr -= (fa_addr % FRAME_SIZE);
		}

		printf("section %i start 0x%08x pyhstart 0x%08x size 0x%08x\n", i, section_start, fa_addr, section_size);

		if(i != LINKER_SECTION_TYPE_TEXT) {
			p_type |= MEMORY_PAGING_PAGE_TYPE_NOEXEC;
		}

		if(i == LINKER_SECTION_TYPE_RODATA || (SYSTEM_INFO->remapped && i == LINKER_SECTION_TYPE_RELOCATION_TABLE)) {
			p_type |= MEMORY_PAGING_PAGE_TYPE_READONLY;
		}

		frame_t f = {fa_addr, section_size / FRAME_SIZE, 0, 0};

		if(memory_paging_add_va_for_frame_ext(p4, section_start, &f, p_type) != 0) {
			return NULL;
		}
	}

	if(memory_paging_add_page_ext(heap, p4, IDT_BASE_ADDRESS, IDT_BASE_ADDRESS, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return NULL;
	}

	uint64_t vfb_fa_addr = SYSTEM_INFO->frame_buffer->physical_base_address;
	uint64_t vfb_va_addr = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vfb_fa_addr);
	SYSTEM_INFO->frame_buffer->virtual_base_address = vfb_va_addr;
	uint64_t vfb_frm_cnt = (SYSTEM_INFO->frame_buffer->buffer_size + FRAME_SIZE - 1) / FRAME_SIZE;

	frame_t f = {vfb_fa_addr, vfb_frm_cnt, 0, 0};

	if(memory_paging_add_va_for_frame_ext(p4, vfb_va_addr, &f, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return NULL;
	}

	return p4;
}

int8_t memory_paging_toggle_attributes_ext(memory_page_table_t* p4, uint64_t virtual_address, memory_paging_page_type_t type) {
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

			if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
				t_p3->pages[p3_idx].writable = ~t_p3->pages[p3_idx].writable;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
				t_p3->pages[p3_idx].no_execute = ~t_p3->pages[p3_idx].no_execute;
			}

			if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
				t_p3->pages[p3_idx].user_accessible = ~t_p3->pages[p3_idx].user_accessible;
			}

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

				if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
					t_p2->pages[p2_idx].writable = ~t_p2->pages[p2_idx].writable;
				}

				if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
					t_p2->pages[p2_idx].no_execute = ~t_p2->pages[p2_idx].no_execute;
				}

				if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
					t_p2->pages[p2_idx].user_accessible = ~t_p2->pages[p2_idx].user_accessible;
				}

			} else {
				t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
				size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

				if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
					return -1;
				}

				if(t_p1->pages[p1_idx].present == 0) {
					return -1;
				}

				if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
					t_p1->pages[p1_idx].writable = ~t_p1->pages[p1_idx].writable;
				}

				if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
					t_p1->pages[p1_idx].no_execute = ~t_p1->pages[p1_idx].no_execute;
				}

				if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
					t_p1->pages[p1_idx].user_accessible = ~t_p1->pages[p1_idx].user_accessible;
				}

			}
		}
	}

	return 0;
}

int8_t memory_paging_delete_page_ext_with_heap(memory_heap_t* heap, memory_page_table_t* p4, uint64_t virtual_address, uint64_t* frame_address){
	UNUSED(heap);

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
			if(frame_address) {
				*frame_address = t_p3->pages[p3_idx].physical_address << 12;
			}

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
				if(frame_address) {
					*frame_address = t_p2->pages[p2_idx].physical_address << 12;
				}

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

				if(frame_address) {
					*frame_address = t_p1->pages[p1_idx].physical_address << 12;
				}

				memory_memclean(&t_p1->pages[p1_idx], sizeof(memory_page_entry_t));

				for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
					if(t_p1->pages[i].present == 1) {
						p1_used = 1;

						break;
					}
				}

				if(p1_used == 0) {
					memory_memclean(&t_p2->pages[p2_idx], sizeof(memory_page_entry_t));

					frame_t f = {(uint64_t)t_p1, 1, 0, 0};
					KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, &f);
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

				frame_t f = {(uint64_t)t_p2, 1, 0, 0};
				KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, &f);
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

			frame_t f = {(uint64_t)t_p3, 1, 0, 0};
			KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, &f);
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

int8_t memory_paging_get_frame_address_ext(memory_page_table_t* p4, uint64_t virtual_address, uint64_t* frame_address){
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
			size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);
			size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);
			*frame_address = (t_p3->pages[p3_idx].physical_address | (p2_idx << 12) | p1_idx) << 12;
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
				size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);
				*frame_address = (t_p2->pages[p2_idx].physical_address | p1_idx) << 12;
			} else {
				t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
				size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

				if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
					return -1;
				}

				if(t_p1->pages[p1_idx].present == 0) {
					return -1;
				}

				*frame_address = t_p1->pages[p1_idx].physical_address << 12;
			}
		}
	}

	return 0;
}

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_t* p4, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type){
	if(frm == NULL) {
		return -1;
	}

	uint64_t frm_addr = frm->frame_address;
	uint64_t frm_cnt = frm->frame_count;

	while(frm_cnt) {
		if(frm_cnt >= 0x200 && (frm_addr % MEMORY_PAGING_PAGE_LENGTH_2M) == 0) { // TODO: check boundaries of huge page.
			if(memory_paging_add_page_with_p4(p4, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
				return -1;
			}

			frm_cnt -= 0x200;
			frm_addr += MEMORY_PAGING_PAGE_LENGTH_2M;
			va_start += MEMORY_PAGING_PAGE_LENGTH_2M;
		} else {
			if(memory_paging_add_page_with_p4(p4, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
				return -1;
			}

			frm_cnt--;
			frm_addr += MEMORY_PAGING_PAGE_LENGTH_4K;
			va_start += MEMORY_PAGING_PAGE_LENGTH_4K;
		}
	}

	return 0;
}
