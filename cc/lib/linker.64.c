/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <linker.h>
#include <cpu.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <systeminfo.h>
#include <video.h>

MODULE("turnstone.lib");

int8_t linker_memcopy_program_and_relink(uint64_t src_program_addr, uint64_t dst_program_addr) {
    program_header_t* src_program = (program_header_t*)src_program_addr;
    program_header_t* dst_program = (program_header_t*)dst_program_addr;

    memory_memcopy(src_program, dst_program, src_program->program_size);

    uint64_t reloc_start = dst_program->reloc_start;
    reloc_start += dst_program_addr;
    uint64_t reloc_count = dst_program->reloc_count;

    linker_direct_relocation_t* relocs = (linker_direct_relocation_t*)reloc_start;

    uint8_t* dst_bytes = (uint8_t*)dst_program;

    for(uint64_t i = 0; i < reloc_count; i++) {
        relocs[i].addend += dst_program_addr;

        if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32) {
            uint32_t* target = (uint32_t*)&dst_bytes[relocs[i].offset];

            *target = (uint32_t)relocs[i].addend;
        } else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S) {
            int32_t* target = (int32_t*)&dst_bytes[relocs[i].offset];

            *target = (int32_t)relocs[i].addend;
        }  else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
            uint64_t* target = (uint64_t*)&dst_bytes[relocs[i].offset];

            *target = relocs[i].addend;
        } else {
            PRINTLOG(LINKER, LOG_FATAL, "unknown relocation 0x%lli type %i", i, relocs[i].relocation_type);

            return -1;
        }
    }

    linker_global_offset_table_entry_t* got_table = (linker_global_offset_table_entry_t*)(dst_program->section_locations[LINKER_SECTION_TYPE_GOT].section_start + dst_program_addr);

    for(uint64_t i = 0; i < dst_program->got_entry_count; i++) {
        if(got_table[i].entry_value != 0) {
            got_table[i].entry_value += dst_program_addr;
        }
    }

    linker_section_locations_t bss_sl = dst_program->section_locations[LINKER_SECTION_TYPE_BSS];
    uint8_t* dst_bss = (uint8_t*)(bss_sl.section_start + dst_program_addr);

    memory_memclean(dst_bss, bss_sl.section_size);

    return 0;
}

#if ___TESTMODE != 1 && ___EFIBUILD != 1

int8_t linker_remap_kernel(void) {
    program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;

    uint64_t data_start = 1 << 30; // 1gib

    linker_section_locations_t sec;
    uint64_t sec_size;
    uint64_t sec_start;
    frame_t f;

    linker_section_locations_t old_section_locations[LINKER_SECTION_TYPE_NR_SECTIONS] = {};

    memory_memcopy(kernel->section_locations, old_section_locations, sizeof(old_section_locations));

    for(uint64_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
        old_section_locations[i].section_start = (old_section_locations[i].section_start >> 12) << 12;
        old_section_locations[i].section_start += SYSTEM_INFO->kernel_start;

        if(old_section_locations[i].section_size % FRAME_SIZE) {
            old_section_locations[i].section_size += (FRAME_SIZE - (old_section_locations[i].section_size % FRAME_SIZE));
        }
    }

    old_section_locations[LINKER_SECTION_TYPE_TEXT].section_start = SYSTEM_INFO->kernel_start;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_TEXT];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_TEXT].section_start = 2 << 20;
    kernel->section_locations[LINKER_SECTION_TYPE_TEXT].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "text sec 0x%llx  0x%llx", 2ULL << 20, sec_size);

    sec = kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE];
    sec_start = sec.section_start;
    sec_size = sec.section_size + (FRAME_SIZE - (sec.section_size % FRAME_SIZE));
    kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_size = sec_size;


    PRINTLOG(LINKER, LOG_TRACE, "reloc sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE];
    sec_start = sec.section_start;
    sec_size = sec.section_size + (FRAME_SIZE - (sec.section_size % FRAME_SIZE));
    kernel->section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE].section_size = sec_size;


    PRINTLOG(LINKER, LOG_TRACE, "got rel reloc sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_GOT];
    sec_start = sec.section_start;
    sec_size = sec.section_size + (FRAME_SIZE - (sec.section_size % FRAME_SIZE));
    kernel->section_locations[LINKER_SECTION_TYPE_GOT].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_GOT].section_size = sec_size;


    PRINTLOG(LINKER, LOG_TRACE, "got sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
        memory_paging_toggle_attributes(SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_READONLY);
        data_start += FRAME_SIZE;
    }

    sec = kernel->section_locations[LINKER_SECTION_TYPE_RODATA];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "rodata sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
        memory_paging_toggle_attributes(SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_READONLY);
        data_start += FRAME_SIZE;
    }

    sec = kernel->section_locations[LINKER_SECTION_TYPE_ROREL];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_ROREL].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_ROREL].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "rorel sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
        memory_paging_toggle_attributes(SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_READONLY);
        data_start += FRAME_SIZE;
    }

    sec = kernel->section_locations[LINKER_SECTION_TYPE_BSS];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_BSS].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_BSS].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "bss sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_DATA];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "data sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_HEAP];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    data_start = 4ULL << 30;
    kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_start = data_start;
    kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_size = sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "heap sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    uint64_t stack_top = 4ULL << 30;

    sec = kernel->section_locations[LINKER_SECTION_TYPE_STACK];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    if(sec_size % FRAME_SIZE) {
        sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
    }

    kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_start = stack_top - sec_size;
    kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_size = sec_size;

    data_start = stack_top - sec_size;

    PRINTLOG(LINKER, LOG_TRACE, "stack sec 0x%llx  0x%llx", data_start, sec_size);

    f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
    f.frame_count = sec_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        return -1;
    }

    data_start += sec_size;

    PRINTLOG(LINKER, LOG_DEBUG, "new sections locations are created on page table");

    cpu_tlb_flush();

    SYSTEM_INFO->remapped = 1;

    frame_t* sysinfo_frms;

    KERNEL_FRAME_ALLOCATOR->rebuild_reserved_mmap(KERNEL_FRAME_ALLOCATOR);

    uint64_t sysinfo_size = SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t) + sizeof(system_info_t);
    uint64_t sysinfo_frm_count = (sysinfo_size + FRAME_SIZE - 1) / FRAME_SIZE;

    while(1) {
        KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, sysinfo_frm_count,
                                                        FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
                                                        &sysinfo_frms, NULL);


        for(uint64_t i = 0; i < sysinfo_frm_count; i++) {
            memory_paging_add_page_ext(NULL, NULL,
                                       sysinfo_frms->frame_address  + i * FRAME_SIZE,
                                       sysinfo_frms->frame_address  + i * FRAME_SIZE,
                                       MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
        }

        KERNEL_FRAME_ALLOCATOR->rebuild_reserved_mmap(KERNEL_FRAME_ALLOCATOR);

        uint64_t new_sysinfo_size = SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t) + sizeof(system_info_t);
        uint64_t new_sysinfo_frm_count = (new_sysinfo_size + FRAME_SIZE - 1) / FRAME_SIZE;

        if(sysinfo_frm_count != new_sysinfo_frm_count) {
            sysinfo_frm_count = new_sysinfo_frm_count;

            KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, sysinfo_frms);
        } else {
            break;
        }
    }

    PRINTLOG(LINKER, LOG_DEBUG, "stored system info frame address 0x%llx", sysinfo_frms->frame_address);

    uint8_t* mmap_data = (uint8_t*)sysinfo_frms->frame_address;
    uint8_t* reserved_mmap_data = (uint8_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size);
    video_frame_buffer_t* vfb = (video_frame_buffer_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size);
    system_info_t* new_sysinfo = (system_info_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t));

    memory_memcopy(SYSTEM_INFO->mmap_data, mmap_data, SYSTEM_INFO->mmap_size);
    memory_memcopy(SYSTEM_INFO->reserved_mmap_data, reserved_mmap_data, SYSTEM_INFO->reserved_mmap_size);
    memory_memcopy(SYSTEM_INFO->frame_buffer, vfb, sizeof(video_frame_buffer_t));
    memory_memcopy(SYSTEM_INFO, new_sysinfo, sizeof(system_info_t));
    new_sysinfo->mmap_data = mmap_data;
    new_sysinfo->reserved_mmap_data = reserved_mmap_data;
    new_sysinfo->frame_buffer = vfb;


    PRINTLOG(LINKER, LOG_DEBUG, "new system info copy created at 0x%p", new_sysinfo);

    linker_direct_relocation_t* relocs = (linker_direct_relocation_t*)(SYSTEM_INFO->kernel_start + kernel->reloc_start);
    linker_direct_relocation_t* got_rel_relocs = (linker_direct_relocation_t*)(SYSTEM_INFO->kernel_start + kernel->got_rel_reloc_start);
    kernel->reloc_start = kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start;
    kernel->got_rel_reloc_start = kernel->section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE].section_start;
    uint64_t kernel_start = SYSTEM_INFO->kernel_start;

    for(uint64_t i = 0; i < kernel->reloc_count; i++)
    {
        linker_section_type_t addend_section = relocs[i].section_type;

        relocs[i].addend += kernel->section_locations[addend_section].section_start - old_section_locations[addend_section].section_start;
    }

    uint64_t got_diff = kernel->section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE].section_start - old_section_locations[LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE].section_start;

    for(uint64_t i = 0; i < kernel->got_rel_reloc_count; i++)
    {
        if(got_rel_relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_GOTPC64) {
            got_rel_relocs[i].addend += got_diff;
        } else {
            linker_section_type_t addend_section = got_rel_relocs[i].section_type;

            got_rel_relocs[i].addend += kernel->section_locations[addend_section].section_start - old_section_locations[addend_section].section_start;

            got_rel_relocs[i].addend -= got_diff;
        }
    }

    linker_global_offset_table_entry_t* got_table = (linker_global_offset_table_entry_t*)(kernel->section_locations[LINKER_SECTION_TYPE_GOT].section_start);

    for(uint64_t i = 0; i < kernel->got_entry_count; i++) {
        if(got_table[i].entry_value != 0) {
            linker_section_type_t addend_section = got_table[i].section_type;

            got_table[i].entry_value += kernel->section_locations[addend_section].section_start - old_section_locations[addend_section].section_start;
        }
    }

    PRINTLOG(LINKER, LOG_DEBUG, "relocs are computed. linking started");

    uint8_t* dst_bytes = (uint8_t*)SYSTEM_INFO->kernel_start;

    for(uint64_t i = 0; i < kernel->reloc_count; i++) {
        //PRINTLOG(LINKER, LOG_INFO, "reloac %lli type %i offset 0x%llx addend 0x%llx\n", i,relocs[i].relocation_type,relocs[i].offset,relocs[i].addend);
        if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32) {
            uint32_t* target = (uint32_t*)&dst_bytes[relocs[i].offset];
            uint32_t target_value = (uint32_t)relocs[i].addend;
            *target = target_value;
        } else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S) {
            int32_t* target = (int32_t*)&dst_bytes[relocs[i].offset];
            int32_t target_value = (int32_t)relocs[i].addend;
            *target = target_value;
        } else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
            uint64_t* target = (uint64_t*)&dst_bytes[relocs[i].offset];
            uint64_t target_value = relocs[i].addend;
            *target = target_value;
        } else {
            //	PRINTLOG(LINKER, LOG_FATAL, "unknown relocation type %i\n", relocs[i].relocation_type);
            while(true) {
                cpu_hlt();
            }
        }
    }

    for(uint64_t i = 0; i < kernel->got_rel_reloc_count; i++) {
        uint64_t* target = (uint64_t*)&dst_bytes[got_rel_relocs[i].offset];
        uint64_t target_value = got_rel_relocs[i].addend;
        *target = target_value;
    }

    /*  while(true) {
          asm volatile ("mov %0, %%rax\n" : : "r" (kernel_start), "D" (new_sysinfo));
          asm volatile ("hlt\n");
       } */

    PRINTLOG(LINKER, LOG_DEBUG, "relocs are finished.");

    sec = kernel->section_locations[LINKER_SECTION_TYPE_BSS];
    sec_start = sec.section_start;
    sec_size = sec.section_size;

    PRINTLOG(LINKER, LOG_DEBUG, "reloc completed\nnew sysinfo at 0x%p.\ncleaning bss and try to re-jump kernel at 0x%llx\n", new_sysinfo, kernel_start);

    memory_memclean((uint8_t*)sec_start, sec_size);
/*
    while(true) {
        asm volatile ("mov %0, %%rax\n" : : "r" (kernel_start), "D" (new_sysinfo));
        asm volatile ("hlt\n");
    }*/

    asm volatile (
        "jmp *%%rax\n"
        : : "D" (new_sysinfo), "a" (kernel_start)
        );

    return 0;
}

#endif
