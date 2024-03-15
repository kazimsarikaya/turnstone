/**
 * @file paging.64.c
 * @brief Paging implementation for x86_64 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <types.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <systeminfo.h>
#include <logging.h>
#include <linker.h>
#include <cpu/descriptor.h>
#include <hashmap.h>

MODULE("turnstone.kernel.memory.paging");

hashmap_t* memory_paging_page_tables = NULL;

uint64_t memory_paging_get_internal_frame(memory_page_table_context_t* table_context);

static void memory_paging_internal_frame_build(memory_page_table_context_t* table_context) {
    frame_t* internal_frms;

    if(!frame_get_allocator() || frame_get_allocator()->allocate_frame_by_count == NULL) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot allocate internal paging frames. frame allocator 0x%p0 is null halting...",
                 frame_get_allocator());
        cpu_hlt();
    }

    if(frame_get_allocator()->allocate_frame_by_count(frame_get_allocator(),
                                                      MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT,
                                                      FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                                      &internal_frms, NULL) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot allocate internal paging frames. Halting...");
        cpu_hlt();
    }

    table_context->internal_frames_2_count = MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT;
    table_context->internal_frames_2_start = internal_frms->frame_address;

#if ___KERNELBUILD == 1
    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms->frame_address),
                                          internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }
#endif

    uint64_t internal_frm_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(table_context->internal_frames_2_start);

    memory_memclean((void*)internal_frm_va, table_context->internal_frames_2_count * FRAME_SIZE);
}

uint64_t memory_paging_get_internal_frame(memory_page_table_context_t* table_context) {
    if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING) {
        uint64_t internal_frm_address = table_context->internal_frames_helper_frame;
        table_context->internal_frames_helper_frame += MEMORY_PAGING_PAGE_SIZE;

        return internal_frm_address;
    }


    if(table_context->internal_frames_1_count == 0) {
        table_context->internal_frames_1_start = table_context->internal_frames_2_start;
        table_context->internal_frames_1_current = table_context->internal_frames_1_start;
        table_context->internal_frames_1_count = table_context->internal_frames_2_count;
        table_context->internal_frames_2_start = 0;
        table_context->internal_frames_2_count = 0;
    }

    if(table_context->internal_frames_1_count < 16) {
        PRINTLOG(PAGING, LOG_DEBUG, "Second internal page frame cache needs refilling");

        if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_UNINITIALIZED) {
            table_context->internal_frame_init_state = MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING;
        }

        memory_paging_internal_frame_build(table_context);

        if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING) {
            table_context->internal_frame_init_state = MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZED;
            table_context->internal_frames_1_start = table_context->internal_frames_2_start;
            table_context->internal_frames_1_current = table_context->internal_frames_1_start;
            table_context->internal_frames_1_count = table_context->internal_frames_2_count;

            memory_paging_internal_frame_build(table_context);

            table_context->internal_frames_helper_frame = table_context->internal_frames_1_current;
            table_context->internal_frames_1_current += 4 * MEMORY_PAGING_PAGE_SIZE;
            table_context->internal_frames_1_count -= 4;

        }

        PRINTLOG(PAGING, LOG_DEBUG, "Second internal page frame cache refilled");
    }

    uint64_t res = table_context->internal_frames_1_current;
    table_context->internal_frames_1_current += MEMORY_PAGING_PAGE_SIZE;
    table_context->internal_frames_1_count--;

    PRINTLOG(PAGING, LOG_DEBUG, "Internal page frame returns frame 0x%llx", res);

    return res;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
memory_page_table_context_t* memory_paging_switch_table(const memory_page_table_context_t* new_table) {
    size_t old_table;
    __asm__ __volatile__ ("mov %%cr3, %0\n"
                          : "=r" (old_table));

    if(new_table != NULL) {
        __asm__ __volatile__ ("mov %0, %%cr3\n" : : "r" (MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(new_table->page_table)));
    }

    if(!memory_paging_page_tables) {
        memory_paging_page_tables = hashmap_integer(128);

        if(!memory_paging_page_tables) {
            PRINTLOG(PAGING, LOG_ERROR, "cannot allocate memory for page tables hashmap");
            cpu_hlt();
        }
    }

    old_table = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(old_table);

    memory_page_table_context_t* old_table_context = (memory_page_table_context_t*)hashmap_get(memory_paging_page_tables, (void*)old_table);

    if(!old_table_context) {

        uint64_t tc_size = sizeof(memory_page_table_context_t);

        if(tc_size % FRAME_SIZE) {
            tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
        }

        old_table_context = memory_malloc_ext(NULL, tc_size, FRAME_SIZE);

        if(!old_table_context) {
            PRINTLOG(PAGING, LOG_ERROR, "cannot allocate memory for page table context");
            cpu_hlt();
        }

        old_table_context->page_table = (memory_page_table_t*)old_table;

        hashmap_put(memory_paging_page_tables, (void*)old_table, old_table_context);
    }

    if(!old_table_context) {
        PRINTLOG(PAGING, LOG_ERROR, "cannot allocate memory for page table context");
        cpu_hlt();
    }

    return old_table_context;
}

int8_t memory_paging_add_page_ext(memory_page_table_context_t* table_context,
                                  uint64_t virtual_address, uint64_t frame_address,
                                  memory_paging_page_type_t type) {

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;
    size_t p3_addr, p2_addr, p1_addr;



    if(table_context == NULL) {
        memory_page_table_context_t* curr = memory_paging_switch_table(NULL);
        table_context = curr;
    }

    memory_page_table_t* p4 = table_context->page_table;

    PRINTLOG(PAGING, LOG_TRACE, "for pt 0x%p adding va 0x%llx to fa 0x%llx", p4, virtual_address, frame_address);

    size_t p4idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4idx >= MEMORY_PAGING_INDEX_COUNT) {
        return -1;
    }

    if(p4->pages[p4idx].present != 1) {
        p3_addr = memory_paging_get_internal_frame(table_context);

        t_p3 = (memory_page_table_t*)p3_addr;
        t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

        memory_memclean(t_p3, FRAME_SIZE);

        p4->pages[p4idx].present = 1;
        p4->pages[p4idx].writable = 1;
        p4->pages[p4idx].physical_address = p3_addr >> 12;

    }
    else {
        uint64_t tmp_pa = p4->pages[p4idx].physical_address;
        t_p3 = (memory_page_table_t*)(tmp_pa << 12);
        t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);
    }

    PRINTLOG(PAGING, LOG_TRACE, "p3 address: 0x%p", t_p3);

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
            p2_addr = memory_paging_get_internal_frame(table_context);

            t_p2 = (memory_page_table_t*)p2_addr;
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

            memory_memclean(t_p2, FRAME_SIZE);

            t_p3->pages[p3idx].present = 1;
            t_p3->pages[p3idx].writable = 1;
            t_p3->pages[p3idx].physical_address = p2_addr >> 12;

        }
    } else {
        if(type & MEMORY_PAGING_PAGE_TYPE_1G) {
            return 0;
        }

        uint64_t tmp_pa = t_p3->pages[p3idx].physical_address;
        t_p2 = (memory_page_table_t*)(tmp_pa << 12);
        t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);
    }

    PRINTLOG(PAGING, LOG_TRACE, "p2 address: 0x%p", t_p2);

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
            p1_addr = memory_paging_get_internal_frame(table_context);

            t_p1 = (memory_page_table_t*)p1_addr;
            t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

            memory_memclean(t_p1, FRAME_SIZE);

            t_p2->pages[p2idx].present = 1;
            t_p2->pages[p2idx].writable = 1;
            t_p2->pages[p2idx].physical_address = p1_addr >> 12;

        }
    } else {
        if(type & MEMORY_PAGING_PAGE_TYPE_2M) {
            return 0;
        }

        uint64_t tmp_pa = t_p2->pages[p2idx].physical_address;
        t_p1 = (memory_page_table_t*)(tmp_pa << 12);
        t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);
    }

    PRINTLOG(PAGING, LOG_TRACE, "p1 address: 0x%p", t_p1);

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

    PRINTLOG(PAGING, LOG_TRACE, "for p4 address: 0x%p va: 0x%llx fa: 0x%llx added", p4, virtual_address, frame_address);

    return 0;
}

int8_t memory_paging_reserve_current_page_table_frames(void) {
    if(!memory_paging_page_tables) {
        memory_paging_page_tables = hashmap_integer(128);

        if(!memory_paging_page_tables) {
            PRINTLOG(PAGING, LOG_ERROR, "failed to allocate memory for page tables hashmap");

            return -1;
        }
    }

    uint64_t tc_size = sizeof(memory_page_table_context_t);

    if(tc_size % FRAME_SIZE) {
        tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
    }

    memory_page_table_context_t* table_context = memory_malloc_ext(NULL, tc_size, FRAME_SIZE);

    if(table_context == NULL) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate memory for page table context");

        return -1;
    }

    program_header_t* program_header = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    memory_page_table_context_t* old_table_context = (memory_page_table_context_t*)program_header->page_table_context_address;

    memory_memcopy(old_table_context, table_context, sizeof(memory_page_table_context_t));

    uint64_t current_cr3 = 0;

    asm volatile ("mov %%cr3, %0" : "=r" (current_cr3));

    uint64_t old_p4 = (uint64_t)table_context->page_table;

    if(old_p4 != current_cr3) {
        PRINTLOG(PAGING, LOG_ERROR, "current cr3 is not equal to old p4");

        return -1;
    }

    old_p4 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(old_p4);

    table_context->page_table = (memory_page_table_t*)old_p4;

    hashmap_put(memory_paging_page_tables, (void*)old_p4, table_context);

    frame_t frm = {0};

    frm.frame_address = table_context->internal_frames_1_start;
    frm.frame_count = table_context->internal_frames_1_count;


    if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &frm) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate internal frames");

        return -1;
    }

    frm.frame_address = table_context->internal_frames_2_start;
    frm.frame_count = table_context->internal_frames_2_count;


    if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &frm) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate internal frames");

        return -1;
    }

    return 0;
}

memory_page_table_context_t* memory_paging_build_empty_table(uint64_t internal_frame_address) {
    PRINTLOG(PAGING, LOG_DEBUG, "building page table started");

    uint64_t tc_size = sizeof(memory_page_table_context_t);

    if(tc_size % FRAME_SIZE) {
        tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
    }

    memory_page_table_context_t* table_context = memory_malloc_ext(NULL, tc_size, FRAME_SIZE);

    if(table_context == NULL) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate memory for page table context");

        return NULL;
    }

    table_context->internal_frames_helper_frame = internal_frame_address;


    uint64_t p4_fa = memory_paging_get_internal_frame(table_context);
    memory_page_table_t* p4 = (memory_page_table_t*)p4_fa;

    p4 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p4);

    memory_memclean(p4, sizeof(memory_page_table_t));

    table_context->page_table = p4;

    PRINTLOG(PAGING, LOG_DEBUG, "p4 address: 0x%p 0x%llx", p4, p4_fa);

    for(int32_t i = 0; i < 4; i++) {
#ifdef ___KERNELBUILD
        memory_paging_add_page_ext(table_context,
                                   MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE),
                                   internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE,
                                   MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
#endif

#ifdef ___EFIBUILD
        memory_paging_add_page_ext(table_context,
                                   MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE) | (64ULL << 40),
                                   internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE,
                                   MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

#endif
    }

#if ___KERNELBUILD == 1
    frame_t internal_frms = {0};

    internal_frms.frame_address = table_context->internal_frames_1_start;
    internal_frms.frame_count = table_context->internal_frames_1_count;

    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms.frame_address),
                                          &internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }

    internal_frms.frame_address = table_context->internal_frames_2_start;
    internal_frms.frame_count = table_context->internal_frames_2_count;

    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms.frame_address),
                                          &internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }
#endif

#if ___EFIBUILD == 1
    frame_t internal_frms = {0};

    internal_frms.frame_address = table_context->internal_frames_1_start;
    internal_frms.frame_count = table_context->internal_frames_1_count;

    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms.frame_address) | (64ULL << 40),
                                          &internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }

    internal_frms.frame_address = table_context->internal_frames_2_start;
    internal_frms.frame_count = table_context->internal_frames_2_count;

    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms.frame_address) | (64ULL << 40),
                                          &internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }
#endif

    return table_context;
}
#pragma GCC diagnostic pop

int8_t memory_paging_clear_page_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_clear_type_t type) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

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
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

    if(p3_idx >= MEMORY_PAGING_INDEX_COUNT) {
        return -1;
    }

    if(t_p3->pages[p3_idx].present == 0) {
        return -1;
    } else {
        if(t_p3->pages[p3_idx].hugepage == 1) {

            if(type & MEMORY_PAGING_CLEAR_TYPE_DIRTY) {
                t_p3->pages[p3_idx].dirty = 0;
            }

            if(type & MEMORY_PAGING_CLEAR_TYPE_ACCESSED) {
                t_p3->pages[p3_idx].accessed = 0;
            }

            cpu_tlb_invalidate(t_p3);

        } else {
            t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

            size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);

            if(p2_idx >= MEMORY_PAGING_INDEX_COUNT) {
                return -1;
            }

            if(t_p2->pages[p2_idx].present == 0) {
                return -1;
            }

            if(t_p2->pages[p2_idx].hugepage == 1) {

                if(type & MEMORY_PAGING_CLEAR_TYPE_DIRTY) {
                    t_p2->pages[p2_idx].dirty = 0;
                }

                if(type & MEMORY_PAGING_CLEAR_TYPE_ACCESSED) {
                    t_p2->pages[p2_idx].accessed = 0;
                }

                cpu_tlb_invalidate(t_p2);


            } else {
                t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
                t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

                size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

                if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
                    return -1;
                }

                if(t_p1->pages[p1_idx].present == 0) {
                    return -1;
                }

                if(type & MEMORY_PAGING_CLEAR_TYPE_DIRTY) {
                    t_p1->pages[p1_idx].dirty = 0;
                }

                if(type & MEMORY_PAGING_CLEAR_TYPE_ACCESSED) {
                    t_p1->pages[p1_idx].accessed = 0;
                }

                cpu_tlb_invalidate(t_p1);


            }
        }
    }

    return 0;
}

int8_t memory_paging_toggle_attributes_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_page_type_t type) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

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

    if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
        p4->pages[p4_idx].user_accessible = ~p4->pages[p4_idx].user_accessible;
    }

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

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

            cpu_tlb_invalidate(t_p3);
            cpu_tlb_invalidate((void*)virtual_address);

        } else {
            if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
                t_p3->pages[p3_idx].user_accessible = ~t_p3->pages[p3_idx].user_accessible;
            }

            t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

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

                cpu_tlb_invalidate(t_p2);
                cpu_tlb_invalidate((void*)virtual_address);


            } else {
                if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
                    t_p2->pages[p2_idx].user_accessible = ~t_p2->pages[p2_idx].user_accessible;
                }

                t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
                t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

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

                cpu_tlb_invalidate(t_p1);
                cpu_tlb_invalidate((void*)virtual_address);


            }
        }
    }

    return 0;
}


int8_t memory_paging_set_user_accessible_ext(memory_page_table_context_t* table_context, uint64_t virtual_address) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

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

    p4->pages[p4_idx].user_accessible = 1;

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

    if(p3_idx >= MEMORY_PAGING_INDEX_COUNT) {
        return -1;
    }

    if(t_p3->pages[p3_idx].present == 0) {
        return -1;
    } else {
        if(t_p3->pages[p3_idx].hugepage == 1) {
            t_p3->pages[p3_idx].user_accessible = 1;

            cpu_tlb_invalidate(t_p3);

        } else {
            t_p3->pages[p3_idx].user_accessible = 1;

            t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

            size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);

            if(p2_idx >= MEMORY_PAGING_INDEX_COUNT) {
                return -1;
            }

            if(t_p2->pages[p2_idx].present == 0) {
                return -1;
            }

            if(t_p2->pages[p2_idx].hugepage == 1) {
                t_p2->pages[p2_idx].user_accessible = 1;

                cpu_tlb_invalidate(t_p2);

            } else {
                t_p2->pages[p2_idx].user_accessible = 1;

                t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
                t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

                size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

                if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
                    return -1;
                }

                if(t_p1->pages[p1_idx].present == 0) {
                    return -1;
                }

                t_p1->pages[p1_idx].user_accessible = 1;


                cpu_tlb_invalidate(t_p1);


            }
        }
    }

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_delete_page_ext_with_heap(memory_page_table_context_t* table_context, uint64_t virtual_address, uint64_t* frame_address){

    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

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
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

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
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

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
                t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

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

                    frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p1), 1, 0, 0};
                    frame_get_allocator()->release_frame(frame_get_allocator(), &f);
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

                frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p2), 1, 0, 0};
                frame_get_allocator()->release_frame(frame_get_allocator(), &f);
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

            frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p3), 1, 0, 0};
            frame_get_allocator()->release_frame(frame_get_allocator(), &f);
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

int8_t memory_paging_get_physical_address_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, uint64_t* physical_address){
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

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
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

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

            *physical_address = (t_p3->pages[p3_idx].physical_address | (p2_idx << 12) | p1_idx) << 12 | (virtual_address & ((1ULL << 30) - 1));

        } else {
            t_p2 = (memory_page_table_t*)((uint64_t)(t_p3->pages[p3_idx].physical_address << 12));
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

            size_t p2_idx = MEMORY_PT_GET_P2_INDEX(virtual_address);

            if(p2_idx >= MEMORY_PAGING_INDEX_COUNT) {
                return -1;
            }

            if(t_p2->pages[p2_idx].present == 0) {
                return -1;
            }

            if(t_p2->pages[p2_idx].hugepage == 1) {
                size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

                *physical_address = (t_p2->pages[p2_idx].physical_address | p1_idx) << 12 | (virtual_address & ((1ULL << 21) - 1));

            } else {
                t_p1 = (memory_page_table_t*)((uint64_t)(t_p2->pages[p2_idx].physical_address << 12));
                t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

                size_t p1_idx = MEMORY_PT_GET_P1_INDEX(virtual_address);

                if(p1_idx >= MEMORY_PAGING_INDEX_COUNT) {
                    return -1;
                }

                if(t_p1->pages[p1_idx].present == 0) {
                    return -1;
                }

                *physical_address = t_p1->pages[p1_idx].physical_address << 12 | (virtual_address & ((1ULL << 12) - 1));
            }
        }
    }

    return 0;
}

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_context_t* table_context, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type){
    if(frm == NULL) {
        return -1;
    }

    uint64_t frm_addr = frm->frame_address;
    uint64_t frm_cnt = frm->frame_count;

    while(frm_cnt) {
        if(frm_cnt >= 0x200 && (frm_addr % MEMORY_PAGING_PAGE_LENGTH_2M) == 0 && (va_start % MEMORY_PAGING_PAGE_LENGTH_2M) == 0) {
            if(memory_paging_add_page_with_p4(table_context, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
                return -1;
            }

            frm_cnt -= 0x200;
            frm_addr += MEMORY_PAGING_PAGE_LENGTH_2M;
            va_start += MEMORY_PAGING_PAGE_LENGTH_2M;
        } else {
            if(memory_paging_add_page_with_p4(table_context, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
                return -1;
            }

            frm_cnt--;
            frm_addr += MEMORY_PAGING_PAGE_LENGTH_4K;
            va_start += MEMORY_PAGING_PAGE_LENGTH_4K;
        }
    }

    return 0;
}

int8_t memory_paging_delete_va_for_frame_ext(memory_page_table_context_t* table_context, uint64_t va_start, frame_t* frm){
    if(frm == NULL) {
        return -1;
    }

    uint64_t frm_addr = frm->frame_address;
    uint64_t frm_cnt = frm->frame_count;

    while(frm_cnt) {
        if(frm_cnt >= 0x200 && (frm_addr % MEMORY_PAGING_PAGE_LENGTH_2M) == 0 && (va_start % MEMORY_PAGING_PAGE_LENGTH_2M) == 0) {
            if(memory_paging_delete_page_ext(table_context, va_start, NULL) != 0) {
                return -1;
            }

            frm_cnt -= 0x200;
            frm_addr += MEMORY_PAGING_PAGE_LENGTH_2M;
            va_start += MEMORY_PAGING_PAGE_LENGTH_2M;
        } else {
            if(memory_paging_delete_page_ext(table_context, va_start, NULL) != 0) {
                return -1;
            }

            frm_cnt--;
            frm_addr += MEMORY_PAGING_PAGE_LENGTH_4K;
            va_start += MEMORY_PAGING_PAGE_LENGTH_4K;
        }
    }

    return 0;
}
