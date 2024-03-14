/**
 * @file hypervisor_ept.64.c
 * @brief Extended Page Table (EPT) setup for hypervisor
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_utils.h>
#include <memory/paging.h>
#include <cpu/task.h>
#include <logging.h>
#include <linker.h>
#include <linker_utils.h>

MODULE("turnstone.hypervisor");


static int8_t hypervisor_ept_add_ept_page(hypervisor_vm_t* vm, uint64_t host_physical, uint64_t guest_physical, boolean_t wb) {
    uint64_t ept_base_fa = vm->ept_pml4_base;
    uint64_t ept_base_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ept_base_fa);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_base_va;

    uint64_t pml4e_index = (guest_physical >> 39) & 0x1FF;

    uint64_t pdpte_va = 0;

    if(pml4e[pml4e_index].read_access == 0 && pml4e[pml4e_index].write_access == 0 && pml4e[pml4e_index].execute_access == 0) {
        frame_t* pdpte_frames = NULL;
        uint64_t pdpte_frames_va = hypervisor_allocate_region(&pdpte_frames, FRAME_SIZE);

        if(pdpte_frames_va == 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate PDPT frames");
            return -1;
        }

        pml4e[pml4e_index].read_access = 1;
        pml4e[pml4e_index].write_access = 1;
        pml4e[pml4e_index].execute_access = 1;
        pml4e[pml4e_index].user_mode_execute_access = 1;

        uint64_t pdpte_fa = pdpte_frames->frame_address;

        pml4e[pml4e_index].address = pdpte_fa >> 12;

        pdpte_va = pdpte_frames_va;
    } else {
        uint64_t pdpte_fa = pml4e[pml4e_index].address;
        pdpte_fa <<= 12;
        pdpte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pdpte_fa);
    }

    uint64_t pdpte_index = (guest_physical >> 30) & 0x1FF;

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)pdpte_va;

    uint64_t pde_va = 0;

    if(pdptes[pdpte_index].read_access == 0 && pdptes[pdpte_index].write_access == 0 && pdptes[pdpte_index].execute_access == 0) {
        frame_t* pde_frames = NULL;
        uint64_t pde_frames_va = hypervisor_allocate_region(&pde_frames, FRAME_SIZE);

        if(pde_frames_va == 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate PDE frames");
            return -1;
        }

        pdptes[pdpte_index].read_access = 1;
        pdptes[pdpte_index].write_access = 1;
        pdptes[pdpte_index].execute_access = 1;
        pdptes[pdpte_index].user_mode_execute_access = 1;

        uint64_t pde_fa = pde_frames->frame_address;

        pdptes[pdpte_index].address = pde_fa >> 12;

        pde_va = pde_frames_va;
    } else {
        uint64_t pde_fa = pdptes[pdpte_index].address;
        pde_fa <<= 12;
        pde_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pde_fa);
    }

    uint64_t pde_index = (guest_physical >> 21) & 0x1FF;

    hypervisor_ept_pde_t* pdes = (hypervisor_ept_pde_t*)pde_va;

    uint64_t pte_va = 0;

    if(pdes[pde_index].read_access == 0 && pdes[pde_index].write_access == 0 && pdes[pde_index].execute_access == 0) {
        frame_t* pte_frames = NULL;
        uint64_t pte_frames_va = hypervisor_allocate_region(&pte_frames, FRAME_SIZE);

        if(pte_frames_va == 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate PTE frames");
            return -1;
        }

        pdes[pde_index].read_access = 1;
        pdes[pde_index].write_access = 1;
        pdes[pde_index].execute_access = 1;
        pdes[pde_index].user_mode_execute_access = 1;

        uint64_t pte_fa = pte_frames->frame_address;

        pdes[pde_index].address = pte_fa >> 12;

        pte_va = pte_frames_va;
    } else {
        uint64_t pte_fa = pdes[pde_index].address;
        pte_fa <<= 12;
        pte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pte_fa);
    }

    uint64_t pte_index = (guest_physical >> 12) & 0x1FF;

    hypervisor_ept_pte_t* ptes = (hypervisor_ept_pte_t*)pte_va;

    ptes[pte_index].read_access = 1;
    ptes[pte_index].write_access = 1;
    ptes[pte_index].execute_access = 1;
    ptes[pte_index].user_mode_execute_access = 1;
    ptes[pte_index].address = host_physical >> 12;
    ptes[pte_index].ignore_pat = 1;
    ptes[pte_index].memory_type = wb?6:0; // if wb then 6 else 0

    return 0;
}

uint64_t hypervisor_ept_setup(hypervisor_vm_t* vm) {
    uint64_t program_page_count = vm->program_size / MEMORY_PAGING_PAGE_LENGTH_4K;
    uint64_t got_page_count = vm->got_size / MEMORY_PAGING_PAGE_LENGTH_4K;
    uint64_t stack_page_count = vm->guest_stack_size / MEMORY_PAGING_PAGE_LENGTH_4K;
    uint64_t heap_page_count = vm->guest_heap_size / MEMORY_PAGING_PAGE_LENGTH_4K;

    uint64_t program_dump_frame_address = vm->program_dump_frame_address;
    uint64_t program_physical_address = vm->program_physical_address;

    frame_t* ept_frames = NULL;
    hypervisor_allocate_region(&ept_frames, FRAME_SIZE);

    vm->ept_pml4_base = ept_frames->frame_address;

    frame_t* descriptor_frames = NULL;
    hypervisor_allocate_region(&descriptor_frames, 4 * FRAME_SIZE);

    for(uint64_t i = 0; i < 4; i++) {
        if(hypervisor_ept_add_ept_page(vm, descriptor_frames->frame_address + i * FRAME_SIZE, 0x1000 * i, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for descriptor");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "descriptor pages added.");

    frame_t* heap_frames = NULL;
    hypervisor_allocate_region(&heap_frames, heap_page_count * FRAME_SIZE);
    vm->guest_heap_physical_base = heap_frames->frame_address;

    for(uint64_t i = 0; i < heap_page_count; i++) {
        uint64_t heap_frame = heap_frames->frame_address + i * FRAME_SIZE;
        if(hypervisor_ept_add_ept_page(vm, heap_frame, heap_frame, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for heap");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "heap pages added.");

    frame_t* stack_frames = NULL;
    hypervisor_allocate_region(&stack_frames, stack_page_count * FRAME_SIZE);
    vm->guest_stack_physical_base = stack_frames->frame_address;

    for(uint64_t i = 0; i < stack_page_count; i++) {
        uint64_t stack_frame = stack_frames->frame_address + i * FRAME_SIZE;
        if(hypervisor_ept_add_ept_page(vm, stack_frame, stack_frame, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for stack");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "stack pages added.");

    PRINTLOG(HYPERVISOR, LOG_TRACE, "got pyhsical address: 0x%llx", vm->got_physical_address);

    uint64_t got_frame_address = vm->got_physical_address;

    for(uint64_t i = 0; i < got_page_count; i++) {
        uint64_t got_base = got_frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K;
        if(hypervisor_ept_add_ept_page(vm, got_base, got_base, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for GOT");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "GOT pages added.");

    for(uint64_t i = 0; i < program_page_count; i++) {
        if(hypervisor_ept_add_ept_page(vm, program_dump_frame_address + i * MEMORY_PAGING_PAGE_LENGTH_4K, program_physical_address + i * MEMORY_PAGING_PAGE_LENGTH_4K, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for program");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "Program pages added.");

    return ept_frames->frame_address;
}

uint64_t hypervisor_ept_guest_to_host(uint64_t ept_base, uint64_t guest_physical) {
    ept_base = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ept_base);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "ept_base: 0x%llx, guest_physical: 0x%llx", ept_base, guest_physical);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_base;

    uint64_t pml4e_index = (guest_physical >> 39) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pml4e_index: 0x%llx", pml4e_index);

    if(pml4e[pml4e_index].read_access == 0 && pml4e[pml4e_index].write_access == 0 && pml4e[pml4e_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t pdpte_fa = pml4e[pml4e_index].address;
    pdpte_fa <<= 12;

    uint64_t pdpte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pdpte_fa);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pdpte_va: 0x%llx", pdpte_va);

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)pdpte_va;

    uint64_t pdpte_index = (guest_physical >> 30) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pdpte_index: 0x%llx", pdpte_index);

    if(pdptes[pdpte_index].read_access == 0 && pdptes[pdpte_index].write_access == 0 && pdptes[pdpte_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t pde_fa = pdptes[pdpte_index].address;
    pde_fa <<= 12;

    uint64_t pde_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pde_fa);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pde_va: 0x%llx", pde_va);

    hypervisor_ept_pde_2mib_t* pdes_2mib = (hypervisor_ept_pde_2mib_t*)pde_va;

    uint64_t pde_index = (guest_physical >> 21) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pde_index: 0x%llx", pde_index);

    if(pdes_2mib[pde_index].must_one) { // 2MiB page entry

        if(pdes_2mib[pde_index].read_access == 0 && pdes_2mib[pde_index].write_access == 0 && pdes_2mib[pde_index].execute_access == 0) {
            return -1ULL;
        }

        uint64_t host_physical = pdes_2mib[pde_index].address;
        host_physical <<= 21;
        host_physical += guest_physical & ((1ULL << 21) - 1);

        return host_physical;

    }

    // 4KiB page entry so go to next level
    hypervisor_ept_pde_t* pdes = (hypervisor_ept_pde_t*)pde_va;

    if(pdes[pde_index].read_access == 0 && pdes[pde_index].write_access == 0 && pdes[pde_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t pte_fa = pdes[pde_index].address;
    pte_fa <<= 12;

    uint64_t pte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pte_fa);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pte_va: 0x%llx", pte_va);

    hypervisor_ept_pte_t* ptes = (hypervisor_ept_pte_t*)pte_va;

    uint64_t pte_index = (guest_physical >> 12) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "pte_index: 0x%llx", pte_index);

    if(ptes[pte_index].read_access == 0 && ptes[pte_index].write_access == 0 && ptes[pte_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t host_physical = ptes[pte_index].address;
    host_physical <<= 12;
    host_physical += guest_physical & ((1ULL << 12) - 1);

    return host_physical;
}

static uint64_t hypervisor_ept_guest_to_host_ensured(hypervisor_vm_t* vm, uint64_t guest_physical) {
    uint64_t host_physical = hypervisor_ept_guest_to_host(vm->ept_pml4_base, guest_physical);

    if(host_physical == -1ULL) {
        frame_t* frame = NULL;

        uint64_t frame_va = hypervisor_allocate_region(&frame, MEMORY_PAGING_PAGE_LENGTH_4K);

        memory_paging_add_va_for_frame(frame_va, frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        if(hypervisor_ept_add_ept_page(vm, frame->frame_address, guest_physical, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for guest_physical: 0x%llx", guest_physical);
            return -1ULL;
        }

        host_physical = frame->frame_address;

        PRINTLOG(HYPERVISOR, LOG_TRACE, "guest_physical: 0x%llx, host_physical: 0x%llx", guest_physical, host_physical);
    }

    return host_physical;
}

static int8_t hypervisor_ept_paging_add_page(hypervisor_vm_t* vm,
                                             uint64_t physical_address, uint64_t virtual_address,
                                             memory_paging_page_type_t type) {
    uint64_t p4_fa = hypervisor_ept_guest_to_host_ensured(vm, 0x4000);
    uint64_t p4_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p4_fa);

    uint64_t p4_index = MEMORY_PT_GET_P4_INDEX(virtual_address);

    memory_page_table_t* p4 = (memory_page_table_t*)p4_va;

    uint64_t p3_fa = 0;

    if(!p4->pages[p4_index].present) {
        uint64_t guest_new_pt_fa = vm->next_page_address;
        vm->next_page_address += MEMORY_PAGING_PAGE_LENGTH_4K;

        uint64_t host_new_pt_fa = hypervisor_ept_guest_to_host_ensured(vm, guest_new_pt_fa);

        p4->pages[p4_index].present = 1;
        p4->pages[p4_index].writable = 1;

        uint64_t tmp_guest_fa = guest_new_pt_fa >> 12;

        p4->pages[p4_index].physical_address = tmp_guest_fa;

        p3_fa = host_new_pt_fa;
    } else {
        uint64_t tmp_guest_fa = p4->pages[p4_index].physical_address;
        tmp_guest_fa <<= 12;

        p3_fa = hypervisor_ept_guest_to_host_ensured(vm, tmp_guest_fa);
    }

    uint64_t p3_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p3_fa);

    uint64_t p3_index = MEMORY_PT_GET_P3_INDEX(virtual_address);

    memory_page_table_t* p3 = (memory_page_table_t*)p3_va;

    uint64_t p2_fa = 0;

    if(!p3->pages[p3_index].present) {
        uint64_t guest_new_pt_fa = vm->next_page_address;
        vm->next_page_address += MEMORY_PAGING_PAGE_LENGTH_4K;

        uint64_t host_new_pt_fa = hypervisor_ept_guest_to_host_ensured(vm, guest_new_pt_fa);

        p3->pages[p3_index].present = 1;
        p3->pages[p3_index].writable = 1;

        uint64_t tmp_guest_fa = guest_new_pt_fa >> 12;

        p3->pages[p3_index].physical_address = tmp_guest_fa;

        p2_fa = host_new_pt_fa;
    } else {
        uint64_t tmp_guest_fa = p3->pages[p3_index].physical_address;
        tmp_guest_fa <<= 12;

        p2_fa = hypervisor_ept_guest_to_host_ensured(vm, tmp_guest_fa);
    }

    uint64_t p2_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p2_fa);

    uint64_t p2_index = MEMORY_PT_GET_P2_INDEX(virtual_address);

    memory_page_table_t* p2 = (memory_page_table_t*)p2_va;

    uint64_t p1_fa = 0;

    if(!p2->pages[p2_index].present) {
        uint64_t guest_new_pt_fa = vm->next_page_address;
        vm->next_page_address += MEMORY_PAGING_PAGE_LENGTH_4K;

        uint64_t host_new_pt_fa = hypervisor_ept_guest_to_host_ensured(vm, guest_new_pt_fa);

        p2->pages[p2_index].present = 1;
        p2->pages[p2_index].writable = 1;

        uint64_t tmp_guest_fa = guest_new_pt_fa >> 12;

        p2->pages[p2_index].physical_address = tmp_guest_fa;

        p1_fa = host_new_pt_fa;
    } else {
        uint64_t tmp_guest_fa = p2->pages[p2_index].physical_address;
        tmp_guest_fa <<= 12;

        p1_fa = hypervisor_ept_guest_to_host_ensured(vm, tmp_guest_fa);
    }

    uint64_t p1_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p1_fa);

    uint64_t p1_index = MEMORY_PT_GET_P1_INDEX(virtual_address);

    memory_page_table_t* p1 = (memory_page_table_t*)p1_va;

    if(!p1->pages[p1_index].present) {
        p1->pages[p1_index].present = 1;

        if(type & MEMORY_PAGING_PAGE_TYPE_READONLY) {
            p1->pages[p1_index].writable = 0;
        } else {
            p1->pages[p1_index].writable = 1;
        }

        if(type & MEMORY_PAGING_PAGE_TYPE_NOEXEC) {
            p1->pages[p1_index].no_execute = 1;
        }

        uint64_t tmp_guest_fa = physical_address >> 12;

        p1->pages[p1_index].physical_address = tmp_guest_fa;
    }

    return 0;
}

int8_t hypervisor_ept_build_tables(hypervisor_vm_t* vm) {
    uint64_t ept_pml4e_base = vm->ept_pml4_base;

    uint64_t gdt_fa = hypervisor_ept_guest_to_host(ept_pml4e_base, 0x2000);
    uint64_t gdt_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(gdt_fa);

    memory_paging_add_page(gdt_va, gdt_fa, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    uint64_t* gdt = (uint64_t*)gdt_va;

    gdt[0] = 0;
    gdt[1] = 0x00209b0000000000ULL;
    gdt[2] = 0x0000930000000000ULL;
    gdt[3] = 0x00008b0030000067ULL;

    vm->next_page_address = 0x5000;

    // main descriptor tables idt, gdt, tss
    hypervisor_ept_paging_add_page(vm, 0x1000, 0x1000, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    hypervisor_ept_paging_add_page(vm, 0x1000, 0x2000, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    hypervisor_ept_paging_add_page(vm, 0x1000, 0x3000, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "descriptor tables added to guest page table.");


    uint64_t got_page_count = vm->got_size / MEMORY_PAGING_PAGE_LENGTH_4K;
    uint64_t stack_page_count = vm->guest_stack_size / MEMORY_PAGING_PAGE_LENGTH_4K;
    uint64_t heap_page_count = vm->guest_heap_size / MEMORY_PAGING_PAGE_LENGTH_4K;

    uint64_t program_dump_frame_address = vm->program_dump_frame_address;
    uint64_t metadata_offset = vm->metadata_physical_address - vm->program_physical_address;
    uint64_t program_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(program_dump_frame_address);
    uint64_t metadata_address = program_va + metadata_offset;

    PRINTLOG(HYPERVISOR, LOG_TRACE, " program physical address 0x%llx metadata physical address 0x%llx",
             vm->program_physical_address, vm->metadata_physical_address);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "program va 0x%llx metadata offset 0x%llx metadata_address: 0x%llx",
             program_va, metadata_offset, metadata_address);

    uint64_t heap_v_base = 4ULL << 40;
    uint64_t heap_p_base = vm->guest_heap_physical_base;

    for(uint64_t i = 0; i < heap_page_count; i++) {
        hypervisor_ept_paging_add_page(vm, heap_p_base, heap_v_base, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
        heap_p_base += MEMORY_PAGING_PAGE_LENGTH_4K;
        heap_v_base += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "heap pages added to guest page table.");

    uint64_t stack_v_base = 4ULL << 40;
    stack_v_base -= stack_page_count * FRAME_SIZE;
    uint64_t stack_p_base = vm->guest_stack_physical_base;

    for(uint64_t i = 0; i < stack_page_count; i++) {
        hypervisor_ept_paging_add_page(vm, stack_p_base, stack_v_base, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
        stack_p_base += MEMORY_PAGING_PAGE_LENGTH_4K;
        stack_v_base += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "stack pages added to guest page table.");

    uint64_t got_v_base = 8ULL << 40;
    uint64_t got_p_base = vm->got_physical_address;

    for(uint64_t i = 0; i < got_page_count; i++) {
        hypervisor_ept_paging_add_page(vm, got_p_base, got_v_base, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
        got_p_base += MEMORY_PAGING_PAGE_LENGTH_4K;
        got_v_base += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "GOT pages added to guest page table. page count 0x%llx", got_page_count);

    linker_metadata_at_memory_t* metadata = (linker_metadata_at_memory_t*)metadata_address;
    metadata++;

    while(true) {
        if(metadata->section.size == 0) {
            break;
        }

        linker_section_type_t section_type = metadata->section.section_type;
        // section size real size so we need to calculate page count
        uint64_t section_page_count = (metadata->section.size + MEMORY_PAGING_PAGE_LENGTH_4K - 1) / MEMORY_PAGING_PAGE_LENGTH_4K;
        memory_paging_page_type_t page_type = MEMORY_PAGING_PAGE_TYPE_READONLY;

        PRINTLOG(HYPERVISOR, LOG_TRACE, "section start 0x%llx section type: %d, section_page_count: 0x%llx",
                 metadata->section.physical_start, section_type, section_page_count);

        if(!(section_type == LINKER_SECTION_TYPE_TEXT || section_type == LINKER_SECTION_TYPE_PLT)) {
            page_type = MEMORY_PAGING_PAGE_TYPE_NOEXEC;
        }

        uint64_t section_v_base = metadata->section.virtual_start;
        uint64_t section_p_base = metadata->section.physical_start;

        for(uint64_t i = 0; i < section_page_count; i++) {
            hypervisor_ept_paging_add_page(vm, section_p_base, section_v_base, page_type);
        }

        metadata++;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "program pages added to guest page table.");

    PRINTLOG(HYPERVISOR, LOG_TRACE, "next page address: 0x%llx", vm->next_page_address);

    // page tables itself
    for(uint64_t i = 0x4000; i < vm->next_page_address; i += MEMORY_PAGING_PAGE_LENGTH_4K) {
        hypervisor_ept_paging_add_page(vm, i, i, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "page tables added to guest page table.");

    return 0;
}
