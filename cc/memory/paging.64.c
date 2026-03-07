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
#include <linker_utils.h>
#include <cpu/descriptor.h>
#include <hashmap.h>

MODULE("turnstone.kernel.memory.paging");

hashmap_t* memory_paging_page_tables = NULL;

static void memory_paging_internal_frame_build(memory_page_table_context_t* table_context) {
    frame_t* internal_frms;

    frame_allocator_t* fa = frame_get_allocator();

    if(!fa || fa->allocate_frame_by_count == NULL) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot allocate internal paging frames. frame allocator 0x%p0 is null halting...",
                 frame_get_allocator());
        cpu_hlt();
    }

    if(fa->allocate_frame_by_count(fa,
                                   MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT,
                                   FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                   &internal_frms, NULL) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot allocate internal paging frames. Halting...");
        cpu_hlt();
    }

    table_context->internal_frames_2_count = MEMORY_PAGING_INTERNAL_FRAMES_MAX_COUNT;
    table_context->internal_frames_2_start = internal_frms->frame_address;

    PRINTLOG(PAGING, LOG_DEBUG, "Internal paging frames allocated at 0x%llx with count 0x%llx", internal_frms->frame_address, internal_frms->frame_count);

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

static uint64_t memory_paging_get_internal_frame(memory_page_table_context_t* table_context) {
    PRINTLOG(PAGING, LOG_TRACE, "Requesting internal page frame for 0x%p", table_context);
    if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING) {
        uint64_t internal_frm_address = table_context->internal_frames_helper_frame;
        table_context->internal_frames_helper_frame += MEMORY_PAGING_PAGE_SIZE;

        PRINTLOG(PAGING, LOG_INFO, "Internal page frame returns frame 0x%llx", internal_frm_address);

        return internal_frm_address;
    }


    if(table_context->internal_frames_1_count == 0) {
        table_context->internal_frames_1_start   = table_context->internal_frames_2_start;
        table_context->internal_frames_1_current = table_context->internal_frames_1_start;
        table_context->internal_frames_1_count   = table_context->internal_frames_2_count;
        table_context->internal_frames_2_start   = 0;
        table_context->internal_frames_2_count   = 0;
    }

    if(table_context->internal_frames_1_count < 16) {
        PRINTLOG(PAGING, LOG_DEBUG, "Second internal page frame cache needs refilling. state: %i", table_context->internal_frame_init_state);

        if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_UNINITIALIZED) {
            table_context->internal_frame_init_state = MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING;
        }

        memory_paging_internal_frame_build(table_context);

        if(table_context->internal_frame_init_state == MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZING) {
            table_context->internal_frame_init_state = MEMORY_PAGING_INTERNAL_FRAME_INIT_STATE_INITIALIZED;
            table_context->internal_frames_1_start   = table_context->internal_frames_2_start;
            table_context->internal_frames_1_current = table_context->internal_frames_1_start;
            table_context->internal_frames_1_count   = table_context->internal_frames_2_count;

            memory_paging_internal_frame_build(table_context);

            // table_context->internal_frames_helper_frame = table_context->internal_frames_1_current;
            // table_context->internal_frames_1_current += 4 * MEMORY_PAGING_PAGE_SIZE;
            // table_context->internal_frames_1_count   -= 4;

            PRINTLOG(PAGING, LOG_TRACE, "Second internal page frame cache refilled for initilized state. state: %i", table_context->internal_frame_init_state);
        }

        PRINTLOG(PAGING, LOG_TRACE, "First internal page starts at 0x%llx with count 0x%llx", table_context->internal_frames_1_start, table_context->internal_frames_1_count);
        PRINTLOG(PAGING, LOG_TRACE, "Second internal page starts at 0x%llx with count 0x%llx", table_context->internal_frames_2_start, table_context->internal_frames_2_count);

        PRINTLOG(PAGING, LOG_TRACE, "Second internal page frame cache refilled");
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
        PRINTLOG(PAGING, LOG_FATAL, "page tables hashmap is null");
        cpu_hlt();
    }

    old_table = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(old_table);

    uint64_t hm_key = (uint64_t)old_table;
    hm_key >>= 12; // first 12 bits are always 0 because of page alignment, so we can ignore them for hashmap key

    memory_page_table_context_t* old_table_context;
    old_table_context = (memory_page_table_context_t*)hashmap_get(memory_paging_page_tables, (void*)hm_key);

    if(!old_table_context) {
        PRINTLOG(PAGING, LOG_WARNING, "page table context for page table 0x%llx not found in hashmap, creating new context", old_table);

        uint64_t tc_size = sizeof(memory_page_table_context_t);

        if(tc_size % FRAME_SIZE) {
            tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
        }

        memory_heap_t* heap = memory_get_default_heap();

        old_table_context = memory_malloc_ext(heap, tc_size, FRAME_SIZE);

        if(!old_table_context) {
            PRINTLOG(PAGING, LOG_ERROR, "cannot allocate memory for page table context");
            cpu_hlt();
        }

        old_table_context->page_table = (memory_page_table_t*)old_table;

        hashmap_put(memory_paging_page_tables, (void*)hm_key, old_table_context);
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

    memory_page_table_context_t* curr = NULL;

#if ___KERNELBUILD == 1
    curr = memory_paging_switch_table(NULL);

    if(!table_context) {
        PRINTLOG(PAGING, LOG_TRACE, "using current page table context for adding page: 0x%p/0x%p", curr, curr->page_table);
        table_context = curr;
    }
#else
    if(!table_context) {
        PRINTLOG(PAGING, LOG_ERROR, "page table context is null");
        return -1;
    }

    curr = table_context;
#endif

    if(!curr) {
        PRINTLOG(PAGING, LOG_ERROR, "cannot get current page table context");
        return -1;
    }

    memory_page_table_t* p4 = table_context->page_table;

    PRINTLOG(PAGING, LOG_TRACE, "for pt 0x%p adding va 0x%llx to fa 0x%llx", p4, virtual_address, frame_address);

    size_t p4idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4idx].present != 1) {
        p3_addr = memory_paging_get_internal_frame(curr);

        t_p3 = (memory_page_table_t*)p3_addr;
        t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

        memory_memclean(t_p3, FRAME_SIZE);

        p4->pages[p4idx].present          = 1;
        p4->pages[p4idx].writable         = 1;
        p4->pages[p4idx].physical_address = p3_addr >> 12;

    }else {
        uint64_t tmp_pa = p4->pages[p4idx].physical_address;
        t_p3 = (memory_page_table_t*)(tmp_pa << 12);
        t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);
    }

    PRINTLOG(PAGING, LOG_TRACE, "p3 address: 0x%p", t_p3);

    size_t p3idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

    if(t_p3->pages[p3idx].present != 1) {
        if(type & MEMORY_PAGING_PAGE_TYPE_1G) {
            t_p3->pages[p3idx].present  = 1;
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

            if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
                t_p3->pages[p3idx].write_through_caching = 1;
            } else {
                t_p3->pages[p3idx].write_through_caching = 0;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
                t_p3->pages[p3idx].disable_cache = 1;
            } else {
                t_p3->pages[p3idx].disable_cache = 0;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
                t_p3->pages[p3idx].global = 1;
            } else {
                t_p3->pages[p3idx].global = 0;
            }

            t_p3->pages[p3idx].physical_address = (frame_address >> 12) & 0xFFFFFC0000;

            return 0;
        } else {
            p2_addr = memory_paging_get_internal_frame(curr);

            t_p2 = (memory_page_table_t*)p2_addr;
            t_p2 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p2);

            memory_memclean(t_p2, FRAME_SIZE);

            t_p3->pages[p3idx].present          = 1;
            t_p3->pages[p3idx].writable         = 1;
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

    if(t_p2->pages[p2idx].present != 1) {
        if(type & MEMORY_PAGING_PAGE_TYPE_2M) {
            t_p2->pages[p2idx].present  = 1;
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

            if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
                t_p2->pages[p2idx].write_through_caching = 1;
            } else {
                t_p2->pages[p2idx].write_through_caching = 0;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
                t_p2->pages[p2idx].disable_cache = 1;
            } else {
                t_p2->pages[p2idx].disable_cache = 0;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
                t_p2->pages[p2idx].global = 1;
            } else {
                t_p2->pages[p2idx].global = 0;
            }

            t_p2->pages[p2idx].physical_address = (frame_address >> 12) & 0xFFFFFFFE00;

            return 0;
        } else {
            p1_addr = memory_paging_get_internal_frame(curr);

            t_p1 = (memory_page_table_t*)p1_addr;
            t_p1 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p1);

            memory_memclean(t_p1, FRAME_SIZE);

            t_p2->pages[p2idx].present          = 1;
            t_p2->pages[p2idx].writable         = 1;
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

        if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
            t_p1->pages[p1idx].write_through_caching = 1;
        } else {
            t_p1->pages[p1idx].write_through_caching = 0;
        }

        if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
            t_p1->pages[p1idx].disable_cache = 1;
        } else {
            t_p1->pages[p1idx].disable_cache = 0;
        }

        if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
            t_p1->pages[p1idx].global = 1;
        } else {
            t_p1->pages[p1idx].global = 0;
        }

        t_p1->pages[p1idx].physical_address = frame_address >> 12;
    }

    PRINTLOG(PAGING, LOG_TRACE, "for p4 address: 0x%p va: 0x%llx fa: 0x%llx added", p4, virtual_address, frame_address);

    return 0;
}

int8_t memory_paging_reserve_current_page_table_frames(void) {
    memory_heap_t* heap = memory_get_default_heap();

    if(!memory_paging_page_tables) {
        memory_paging_page_tables = hashmap_integer_with_heap(heap, 128);

        if(!memory_paging_page_tables) {
            PRINTLOG(PAGING, LOG_ERROR, "failed to allocate memory for page tables hashmap");

            return -1;
        }
    }

    uint64_t tc_size = sizeof(memory_page_table_context_t);

    if(tc_size % FRAME_SIZE) {
        tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
    }

    memory_page_table_context_t* table_context = memory_malloc_ext(heap, tc_size, FRAME_SIZE);

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
    uint64_t hm_key = (uint64_t)old_p4;
    hm_key >>= 12; // first 12 bits are always 0 because of page alignment, so we can ignore them for hashmap key

    table_context->page_table = (memory_page_table_t*)old_p4;

    hashmap_put(memory_paging_page_tables, (void*)hm_key, table_context);

    frame_t frm = {0};

    frm.frame_address = table_context->internal_frames_1_start;
    frm.frame_count   = table_context->internal_frames_1_count;


    if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &frm) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate internal frames");

        return -1;
    }

    frm.frame_address = table_context->internal_frames_2_start;
    frm.frame_count   = table_context->internal_frames_2_count;


    if(frame_get_allocator()->allocate_frame(frame_get_allocator(), &frm) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate internal frames");

        return -1;
    }

    return 0;
}
memory_page_table_context_t* memory_paging_create_empty_userspace_table(
    uint64_t gdt_fa_location, uint64_t gdt_size,
    uint64_t tss_fa_location, uint64_t tss_size,
    uint64_t stack_bottom_fa_location, uint64_t stack_size
    ) {

    memory_page_table_context_t* new_table_context = memory_paging_build_empty_table(0);

    if(!new_table_context) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to build empty page table context for userspace");

        return NULL;
    }

    const linker_metadata_at_memory_t* module_or_section = linker_get_module_at_memory(SYSTEM_INFO->interrupt_handlers_module_id);
    const linker_section_at_memory_t* text_section       = NULL;
    const linker_section_at_memory_t* bss_section        = NULL;

    module_or_section++;
    while(module_or_section->section.size) {

        if(module_or_section->section.section_type == LINKER_SECTION_TYPE_TEXT) {
            text_section = &module_or_section->section;
        }

        if(module_or_section->section.section_type == LINKER_SECTION_TYPE_BSS) {
            bss_section = &module_or_section->section;
        }

        module_or_section++;
    }

    if(!text_section || !bss_section) {
        PRINTLOG(PAGING, LOG_ERROR, "cannot find text or bss section for interrupt handlers module: text section: 0x%p bss section: 0x%p", text_section, bss_section);
        cpu_hlt();

        return NULL;
    }

    PRINTLOG(PAGING, LOG_DEBUG, "mapping interrupt handlers text section: va: 0x%llx fa: 0x%llx size: 0x%llx",
             text_section->virtual_start, text_section->physical_start, text_section->size);
    PRINTLOG(PAGING, LOG_DEBUG, "mapping interrupt handlers bss section: va: 0x%llx fa: 0x%llx size: 0x%llx",
             bss_section->virtual_start, bss_section->physical_start, bss_section->size);

    for(uint64_t offset = 0; offset < text_section->size; offset += MEMORY_PAGING_PAGE_SIZE) {
        PRINTLOG(PAGING, LOG_DEBUG, "mapping interrupt handlers text section page: 0x%llx 0x%llx",
                 text_section->virtual_start + offset,
                 text_section->physical_start + offset);
        memory_paging_add_page_ext(new_table_context,
                                   text_section->virtual_start + offset,
                                   text_section->physical_start + offset,
                                   MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY);
    }

    for(uint64_t offset = 0; offset < bss_section->size; offset += MEMORY_PAGING_PAGE_SIZE) {
        PRINTLOG(PAGING, LOG_DEBUG, "mapping interrupt handlers bss section page: 0x%llx 0x%llx",
                 bss_section->virtual_start + offset,
                 bss_section->physical_start + offset);
        memory_paging_add_page_ext(new_table_context,
                                   bss_section->virtual_start + offset,
                                   bss_section->physical_start + offset,
                                   MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC | MEMORY_PAGING_PAGE_TYPE_READONLY);
    }

    frame_t tmp_frame = {0};

    uint16_t idt_size = sizeof(descriptor_idt_t) * 256;
    tmp_frame.frame_address = IDT_BASE_ADDRESS;
    tmp_frame.frame_count   = (idt_size + FRAME_SIZE - 1) / FRAME_SIZE;

    memory_paging_add_va_for_frame_ext(new_table_context,
                                       tmp_frame.frame_address,
                                       &tmp_frame,
                                       MEMORY_PAGING_PAGE_TYPE_NOEXEC);


    tmp_frame.frame_address = gdt_fa_location;
    tmp_frame.frame_count   = (gdt_size + FRAME_SIZE - 1) / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(new_table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tmp_frame.frame_address),
                                          &tmp_frame,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to map gdt frames for userspace page table");
        cpu_hlt();

        return NULL;
    }

    tmp_frame.frame_address = tss_fa_location;
    tmp_frame.frame_count   = (tss_size + FRAME_SIZE - 1) / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(new_table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tmp_frame.frame_address),
                                          &tmp_frame,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to map tss frames for userspace page table");
        cpu_hlt();

        return NULL;
    }

    tmp_frame.frame_address = stack_bottom_fa_location;
    tmp_frame.frame_count   = (stack_size + FRAME_SIZE - 1) / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(new_table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tmp_frame.frame_address),
                                          &tmp_frame,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to map stack frames for userspace page table");
        cpu_hlt();

        return NULL;
    }

    uint64_t hm_key = (uint64_t)new_table_context->page_table;
    hm_key >>= 12; // first 12 bits are always 0 because of page alignment, so we can ignore them for hashmap key

    hashmap_put(memory_paging_page_tables, (void*)hm_key, new_table_context);


    return new_table_context;
}


memory_page_table_context_t* memory_paging_build_empty_table(uint64_t internal_frame_address) {
    PRINTLOG(PAGING, LOG_DEBUG, "building page table started");

    memory_heap_t* heap = memory_get_default_heap();

    uint64_t tc_size = sizeof(memory_page_table_context_t);

    if(tc_size % FRAME_SIZE) {
        tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
    }

    memory_page_table_context_t* table_context = memory_malloc_ext(heap, tc_size, FRAME_SIZE);

    if(table_context == NULL) {
        PRINTLOG(PAGING, LOG_ERROR, "failed to allocate memory for page table context");

        return NULL;
    }

    table_context->internal_frames_helper_frame = internal_frame_address;

#if ___KERNELBUILD == 1
    memory_page_table_context_t* current_table_context = memory_paging_switch_table(NULL);

    uint64_t p4_fa = memory_paging_get_internal_frame(current_table_context);
#endif

#if ___EFIBUILD == 1
    uint64_t p4_fa = memory_paging_get_internal_frame(table_context);
#endif

    memory_page_table_t* p4 = (memory_page_table_t*)p4_fa;

    p4 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p4);

    table_context->page_table = p4;

    PRINTLOG(PAGING, LOG_DEBUG, "p4 address: 0x%p 0x%llx", p4, p4_fa);

#if ___EFIBUILD == 1
    for(int32_t i = 0; i < 4; i++) {
        memory_paging_add_page_ext(table_context,
                                   MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE) | (64ULL << 40),
                                   internal_frame_address + i * MEMORY_PAGING_PAGE_SIZE,
                                   MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    }

    frame_t internal_frms = {0};

    internal_frms.frame_address = table_context->internal_frames_1_start;
    internal_frms.frame_count   = table_context->internal_frames_1_count;

    if(memory_paging_add_va_for_frame_ext(table_context,
                                          MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(internal_frms.frame_address) | (64ULL << 40),
                                          &internal_frms,
                                          MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(PAGING, LOG_PANIC, "cannot map internal paging frames. Halting...");
        cpu_hlt();
    }

    internal_frms.frame_address = table_context->internal_frames_2_start;
    internal_frms.frame_count   = table_context->internal_frames_2_count;

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_clear_page_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_clear_type_t type) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;

    size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4_idx].present == 0) {
        return -1;
    }

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

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
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_toggle_attributes_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, memory_paging_page_type_t type) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;

    size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4_idx].present == 0) {
        return -1;
    }

    if(type & MEMORY_PAGING_PAGE_TYPE_USER_ACCESSIBLE) {
        p4->pages[p4_idx].user_accessible = ~p4->pages[p4_idx].user_accessible;
    }

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

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

            if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
                t_p3->pages[p3_idx].write_through_caching = ~t_p3->pages[p3_idx].write_through_caching;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
                t_p3->pages[p3_idx].disable_cache = ~t_p3->pages[p3_idx].disable_cache;
            }

            if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
                t_p3->pages[p3_idx].global = ~t_p3->pages[p3_idx].global;
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

                if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
                    t_p2->pages[p2_idx].write_through_caching = ~t_p2->pages[p2_idx].write_through_caching;
                }

                if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
                    t_p2->pages[p2_idx].disable_cache = ~t_p2->pages[p2_idx].disable_cache;
                }

                if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
                    t_p2->pages[p2_idx].global = ~t_p2->pages[p2_idx].global;
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

                if(type & MEMORY_PAGING_PAGE_TYPE_WRITE_THROUGH) {
                    t_p1->pages[p1_idx].write_through_caching = ~t_p1->pages[p1_idx].write_through_caching;
                }

                if(type & MEMORY_PAGING_PAGE_TYPE_DISABLE_CACHE) {
                    t_p1->pages[p1_idx].disable_cache = ~t_p1->pages[p1_idx].disable_cache;
                }

                if(type & MEMORY_PAGING_PAGE_TYPE_GLOBAL) {
                    t_p1->pages[p1_idx].global = ~t_p1->pages[p1_idx].global;
                }

                cpu_tlb_invalidate(t_p1);
                cpu_tlb_invalidate((void*)virtual_address);


            }
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_set_user_accessible_ext(memory_page_table_context_t* table_context, uint64_t virtual_address) {
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;

    size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4_idx].present == 0) {
        return -1;
    }

    p4->pages[p4_idx].user_accessible = 1;

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

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
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_delete_page_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, uint64_t* frame_address){

    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;

    boolean_t p1_used = false, p2_used = false, p3_used = false;

    size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4_idx].present == 0) {
        return -1;
    }

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

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

                if(t_p1->pages[p1_idx].present == 0) {
                    return -1;
                }

                if(frame_address) {
                    *frame_address = t_p1->pages[p1_idx].physical_address << 12;
                }

                memory_memclean(&t_p1->pages[p1_idx], sizeof(memory_page_entry_t));

                for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
                    if(t_p1->pages[i].present == 1) {
                        p1_used = true;

                        break;
                    }
                }

                if(!p1_used) {
                    memory_memclean(&t_p2->pages[p2_idx], sizeof(memory_page_entry_t));

                    frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p1), 1, 0, 0};
                    frame_get_allocator()->release_frame(frame_get_allocator(), &f);
                }

            }

            for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
                if(t_p2->pages[i].present == 1) {
                    p2_used = true;

                    break;
                }
            }

            if(!p2_used) {
                memory_memclean(&t_p3->pages[p3_idx], sizeof(memory_page_entry_t));

                frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p2), 1, 0, 0};
                frame_get_allocator()->release_frame(frame_get_allocator(), &f);
            }
        }

        for(size_t i = 0; i < MEMORY_PAGING_INDEX_COUNT; i++) {
            if(t_p3->pages[i].present == 1) {
                p3_used = true;

                break;
            }
        }

        if(!p3_used) {
            memory_memclean(&p4->pages[p4_idx], sizeof(memory_page_entry_t));

            frame_t f = {MEMORY_PAGING_GET_FA_FOR_RESERVED_VA((uint64_t)t_p3), 1, 0, 0};
            frame_get_allocator()->release_frame(frame_get_allocator(), &f);
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t memory_paging_get_physical_address_ext(memory_page_table_context_t* table_context, uint64_t virtual_address, uint64_t* physical_address){
    if(table_context == NULL) {
        table_context = memory_paging_switch_table(NULL);
    }

    memory_page_table_t* p4 = table_context->page_table;

    memory_page_table_t* t_p3;
    memory_page_table_t* t_p2;
    memory_page_table_t* t_p1;

    size_t p4_idx = MEMORY_PT_GET_P4_INDEX(virtual_address);

    if(p4->pages[p4_idx].present == 0) {
        return -1;
    }

    t_p3 = (memory_page_table_t*)((uint64_t)(p4->pages[p4_idx].physical_address << 12));
    t_p3 = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(t_p3);

    size_t p3_idx = MEMORY_PT_GET_P3_INDEX(virtual_address);

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

                if(t_p1->pages[p1_idx].present == 0) {
                    return -1;
                }

                *physical_address = t_p1->pages[p1_idx].physical_address << 12 | (virtual_address & ((1ULL << 12) - 1));
            }
        }
    }

    return 0;
}
#pragma GCC diagnostic pop

int8_t memory_paging_add_va_for_frame_ext(memory_page_table_context_t* table_context, uint64_t va_start, frame_t* frm, memory_paging_page_type_t type){
    if(frm == NULL) {
        return -1;
    }

    uint64_t frm_addr = frm->frame_address;
    uint64_t frm_cnt  = frm->frame_count;

    while(frm_cnt) {
        if(frm_cnt >= 0x200 && (frm_addr % MEMORY_PAGING_PAGE_LENGTH_2M) == 0 && (va_start % MEMORY_PAGING_PAGE_LENGTH_2M) == 0) {
            if(memory_paging_add_page_ext(table_context, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_2M) != 0) {
                return -1;
            }

            frm_cnt  -= 0x200;
            frm_addr += MEMORY_PAGING_PAGE_LENGTH_2M;
            va_start += MEMORY_PAGING_PAGE_LENGTH_2M;
        } else {
            if(memory_paging_add_page_ext(table_context, va_start, frm_addr, type | MEMORY_PAGING_PAGE_TYPE_4K) != 0) {
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
    uint64_t frm_cnt  = frm->frame_count;

    while(frm_cnt) {
        if(frm_cnt >= 0x200 && (frm_addr % MEMORY_PAGING_PAGE_LENGTH_2M) == 0 && (va_start % MEMORY_PAGING_PAGE_LENGTH_2M) == 0) {
            if(memory_paging_delete_page_ext(table_context, va_start, NULL) != 0) {
                return -1;
            }

            frm_cnt  -= 0x200;
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
