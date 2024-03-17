/**
 * @file hypervisor_ept.64.c
 * @brief Extended Page Table (EPT) setup for hypervisor
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <memory/paging.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <cpu.h>
#include <logging.h>
#include <linker.h>
#include <linker_utils.h>
#include <list.h>

MODULE("turnstone.hypervisor");

static void hypervisor_ept_invept(uint64_t type) {
    uint128_t eptp = vmx_read(VMX_CTLS_EPTP);
    asm volatile ("invept (%0), %1" : : "r" (&eptp), "r" (type) : "memory");
}

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

        list_list_insert(vm->ept_frames, pdpte_frames);

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

        list_list_insert(vm->ept_frames, pde_frames);

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

        list_list_insert(vm->ept_frames, pte_frames);

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
static int8_t hypervisor_ept_del_ept_page(hypervisor_vm_t* vm, uint64_t host_physical, uint64_t guest_physical) {
    uint64_t ept_base_fa = vm->ept_pml4_base;
    uint64_t ept_base_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ept_base_fa);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_base_va;

    uint64_t pml4e_index = (guest_physical >> 39) & 0x1FF;

    uint64_t pdpte_va = 0;

    if(pml4e[pml4e_index].read_access == 0 && pml4e[pml4e_index].write_access == 0 && pml4e[pml4e_index].execute_access == 0) {
        return 0;
    } else {
        uint64_t pdpte_fa = pml4e[pml4e_index].address;
        pdpte_fa <<= 12;
        pdpte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pdpte_fa);
    }

    uint64_t pdpte_index = (guest_physical >> 30) & 0x1FF;

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)pdpte_va;

    uint64_t pde_va = 0;

    if(pdptes[pdpte_index].read_access == 0 && pdptes[pdpte_index].write_access == 0 && pdptes[pdpte_index].execute_access == 0) {
        return 0;
    } else {
        uint64_t pde_fa = pdptes[pdpte_index].address;
        pde_fa <<= 12;
        pde_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pde_fa);
    }

    uint64_t pde_index = (guest_physical >> 21) & 0x1FF;

    hypervisor_ept_pde_t* pdes = (hypervisor_ept_pde_t*)pde_va;

    uint64_t pte_va = 0;

    if(pdes[pde_index].read_access == 0 && pdes[pde_index].write_access == 0 && pdes[pde_index].execute_access == 0) {
        return 0;
    } else {
        uint64_t pte_fa = pdes[pde_index].address;
        pte_fa <<= 12;
        pte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pte_fa);
    }

    uint64_t pte_index = (guest_physical >> 12) & 0x1FF;

    hypervisor_ept_pte_t* ptes = (hypervisor_ept_pte_t*)pte_va;

    if(ptes[pte_index].read_access == 0 && ptes[pte_index].write_access == 0 && ptes[pte_index].execute_access == 0) {
        return 0;
    }

    uint64_t pte_address = ptes[pte_index].address;
    pte_address <<= 12;

    if(pte_address == host_physical) {
        memory_memclean(&ptes[pte_index], sizeof(hypervisor_ept_pte_t));
    }

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

    list_list_insert(vm->ept_frames, ept_frames);

    vm->ept_pml4_base = ept_frames->frame_address;

    frame_t* descriptor_frames = NULL;
    hypervisor_allocate_region(&descriptor_frames, 4 * FRAME_SIZE);

    list_list_insert(vm->ept_frames, descriptor_frames);

    for(uint64_t i = 0; i < 4; i++) {
        if(hypervisor_ept_add_ept_page(vm, descriptor_frames->frame_address + i * FRAME_SIZE, 0x1000 * i, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add EPT page for descriptor");
            return -1;
        }
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "descriptor pages added.");

    frame_t* heap_frames = NULL;
    hypervisor_allocate_region(&heap_frames, heap_page_count * FRAME_SIZE);

    list_list_insert(vm->ept_frames, heap_frames);

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

    list_list_insert(vm->ept_frames, stack_frames);

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

        list_list_insert(vm->ept_frames, frame);

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

static int8_t hypervisor_ept_paging_del_page(hypervisor_vm_t* vm,
                                             uint64_t physical_address, uint64_t virtual_address) {
    uint64_t p4_fa = hypervisor_ept_guest_to_host_ensured(vm, 0x4000);
    uint64_t p4_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p4_fa);

    uint64_t p4_index = MEMORY_PT_GET_P4_INDEX(virtual_address);

    memory_page_table_t* p4 = (memory_page_table_t*)p4_va;

    uint64_t p3_fa = 0;

    if(!p4->pages[p4_index].present) {
        return 0;
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
        return 0;
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
        return 0;
    } else {
        uint64_t tmp_guest_fa = p2->pages[p2_index].physical_address;
        tmp_guest_fa <<= 12;

        p1_fa = hypervisor_ept_guest_to_host_ensured(vm, tmp_guest_fa);
    }

    uint64_t p1_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p1_fa);

    uint64_t p1_index = MEMORY_PT_GET_P1_INDEX(virtual_address);

    memory_page_table_t* p1 = (memory_page_table_t*)p1_va;

    uint64_t pte_address = p1->pages[p1_index].physical_address;
    pte_address <<= 12;

    if(p1->pages[p1_index].present && pte_address == physical_address) {
        PRINTLOG(HYPERVISOR, LOG_TRACE, "p4 fa: 0x%llx, p3 fa: 0x%llx, p2 fa: 0x%llx, p1 fa: 0x%llx",
                 p4_fa, p3_fa, p2_fa, p1_fa);
        PRINTLOG(HYPERVISOR, LOG_TRACE, "p4 index: 0x%llx, p3 index: 0x%llx, p2 index: 0x%llx, p1 index: 0x%llx",
                 p4_index, p3_index, p2_index, p1_index);
        memory_memclean(&p1->pages[p1_index], sizeof(memory_page_entry_t));
    }

    return 0;
}

static int8_t hypervisor_ept_paging_get_guest_physical(hypervisor_vm_t* vm, uint64_t virtual_address) {
    uint64_t p4_fa = hypervisor_ept_guest_to_host_ensured(vm, 0x4000);
    uint64_t p4_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p4_fa);

    uint64_t p4_index = MEMORY_PT_GET_P4_INDEX(virtual_address);

    memory_page_table_t* p4 = (memory_page_table_t*)p4_va;

    uint64_t p3_fa = 0;

    if(!p4->pages[p4_index].present) {
        return 0;
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
        return 0;
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
        return 0;
    } else {
        uint64_t tmp_guest_fa = p2->pages[p2_index].physical_address;
        tmp_guest_fa <<= 12;

        p1_fa = hypervisor_ept_guest_to_host_ensured(vm, tmp_guest_fa);
    }

    uint64_t p1_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(p1_fa);

    uint64_t p1_index = MEMORY_PT_GET_P1_INDEX(virtual_address);

    memory_page_table_t* p1 = (memory_page_table_t*)p1_va;

    uint64_t pte_address = p1->pages[p1_index].physical_address;
    pte_address <<= 12;

    if(p1->pages[p1_index].present) {
        return pte_address;
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
            page_type |= MEMORY_PAGING_PAGE_TYPE_NOEXEC;
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

    hypervisor_ept_invept(1);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "page tables added to guest page table.");

    return 0;
}

int8_t hypervisor_ept_merge_module(hypervisor_vm_t* vm, hypervisor_vm_module_load_t* module_load) {
    // operation order
    // 1. remove old got pages from page table
    // 2. remove old got pages from ept
    // 3. add new got pages to page table
    // 4. add new got pages to ept
    // 5. add new module pages to ept
    // 6. add new module pages to page table with traversing section list

    uint64_t old_got_physical_address = vm->got_physical_address;
    uint64_t old_got_size = vm->got_size;
    uint64_t old_got_page_count = old_got_size / MEMORY_PAGING_PAGE_LENGTH_4K;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "old got physical address: 0x%llx, old got size: 0x%llx, old got page count: 0x%llx",
             old_got_physical_address, old_got_size, old_got_page_count);

    uint64_t got_v_base = 8ULL << 40;

    for(uint64_t i = 0; i < old_got_page_count; i++) {
        if(hypervisor_ept_paging_del_page(vm, old_got_physical_address, got_v_base) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to remove old got pages from page table");
            return -1;
        }

        got_v_base += MEMORY_PAGING_PAGE_LENGTH_4K;
        old_got_physical_address += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    old_got_physical_address = vm->got_physical_address;

    for(uint64_t i = 0; i < old_got_page_count; i++) {
        if(hypervisor_ept_del_ept_page(vm, old_got_physical_address, old_got_physical_address) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to remove old got pages from ept");
            return -1;
        }

        old_got_physical_address += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    old_got_physical_address = vm->got_physical_address;

    frame_t got_frame = {.frame_address = old_got_physical_address, .frame_count = old_got_page_count};

    uint64_t frame_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(got_frame.frame_address);

    memory_memclean((void*)frame_va, FRAME_SIZE * got_frame.frame_count);

    if(memory_paging_delete_va_for_frame_ext(NULL, frame_va, &got_frame) != 0 ) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot remove pages for stack at va 0x%llx", frame_va);
        return -1;
    }

    if(frame_get_allocator()->release_frame(frame_get_allocator(), &got_frame) != 0) {
        PRINTLOG(TASKING, LOG_ERROR, "cannot release stack with frames at 0x%llx with count 0x%llx",
                 got_frame.frame_address, got_frame.frame_count);
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "released 0x%llx 0x%llx", got_frame.frame_address, got_frame.frame_count);

    uint64_t new_got_physical_address = module_load->new_got_physical_address;
    uint64_t new_got_size = module_load->new_got_size;
    uint64_t new_got_page_count = new_got_size / MEMORY_PAGING_PAGE_LENGTH_4K;

    for(uint64_t i = 0; i < new_got_page_count; i++) {
        if(hypervisor_ept_add_ept_page(vm, new_got_physical_address, new_got_physical_address, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add new got pages to page table");
            return -1;
        }

        new_got_physical_address += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    new_got_physical_address = module_load->new_got_physical_address;
    uint64_t got_base = 8ULL << 40;

    for(uint64_t i = 0; i < new_got_page_count; i++) {
        if(hypervisor_ept_paging_add_page(vm, new_got_physical_address, got_base, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add new got pages to ept");
            return -1;
        }

        new_got_physical_address += MEMORY_PAGING_PAGE_LENGTH_4K;
        got_base += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "got pages switched on ept and page table.");

    uint64_t module_dump_frame_address = module_load->module_dump_physical_address;
    uint64_t module_physical_address = module_load->module_physical_address;
    uint64_t module_size = module_load->module_size;
    uint64_t module_page_count = module_size / MEMORY_PAGING_PAGE_LENGTH_4K;

    for(uint64_t i = 0; i < module_page_count; i++) {
        if(hypervisor_ept_add_ept_page(vm, module_dump_frame_address, module_physical_address, true) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add new module pages to ept");
            return -1;
        }

        module_dump_frame_address += MEMORY_PAGING_PAGE_LENGTH_4K;
        module_physical_address += MEMORY_PAGING_PAGE_LENGTH_4K;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "module pages added to ept.");

    module_dump_frame_address = module_load->module_dump_physical_address;
    uint64_t metadata_offset = module_load->metadata_physical_address - module_load->module_dump_physical_address;
    uint64_t module_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(module_dump_frame_address);
    uint64_t metadata_address = module_va + metadata_offset;

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

        if(!(section_type == LINKER_SECTION_TYPE_TEXT || section_type == LINKER_SECTION_TYPE_PLT)) {
            page_type |= MEMORY_PAGING_PAGE_TYPE_NOEXEC;
        }

        if(section_type == LINKER_SECTION_TYPE_BSS ||
           section_type == LINKER_SECTION_TYPE_DATA ||
           section_type == LINKER_SECTION_TYPE_DATARELOC) {
            list_list_insert(vm->read_only_frames, metadata);
        }

        PRINTLOG(HYPERVISOR, LOG_TRACE, "section start 0x%llx section type: %d, section_page_count: 0x%llx page type: 0x%x",
                 metadata->section.physical_start, section_type, section_page_count, page_type);

        uint64_t section_v_base = metadata->section.virtual_start;
        uint64_t section_p_base = metadata->section.physical_start;

        for(uint64_t i = 0; i < section_page_count; i++) {
            hypervisor_ept_paging_add_page(vm, section_p_base, section_v_base, page_type);
            section_p_base += MEMORY_PAGING_PAGE_LENGTH_4K;
            section_v_base += MEMORY_PAGING_PAGE_LENGTH_4K;
        }

        metadata++;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "module pages added to guest page table.");

    PRINTLOG(HYPERVISOR, LOG_TRACE, "next page address: 0x%llx", vm->next_page_address);

    // page tables itself
    for(uint64_t i = 0x4000; i < vm->next_page_address; i += MEMORY_PAGING_PAGE_LENGTH_4K) {
        hypervisor_ept_paging_add_page(vm, i, i, MEMORY_PAGING_PAGE_TYPE_NOEXEC);
    }

    uint64_t host_page = hypervisor_ept_guest_to_host(vm->ept_pml4_base, 0x4000);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "page tables added to guest page table. cr3 on host: 0x%llx", host_page);

    hypervisor_ept_invept(1);

    vm->got_physical_address = module_load->new_got_physical_address;
    vm->got_size = module_load->new_got_size;

    return 0;
}

uint64_t hypervisor_ept_page_fault_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t error_code = vmexit_info->interrupt_error_code;
    hypervisor_vm_t* vm = task_get_vm();

    interrupt_errorcode_pagefault_t pagefault_error = {.bits = error_code};

    if(pagefault_error.fields.present && pagefault_error.fields.write) {
        uint64_t error_address = vmexit_info->exit_qualification; // exit qualification is the address that caused the page fault

        linker_metadata_at_memory_t md = {.section = {.virtual_start = error_address, .size = 1}};

        uint64_t pos = 0;

        if(list_get_position(vm->read_only_frames, &md, &pos) == 0) {
            PRINTLOG(HYPERVISOR, LOG_TRACE, "Write access to read only memory at 0x%llx", error_address);

            const linker_metadata_at_memory_t* res_md = list_get_data_at_position(vm->read_only_frames, pos);

            list_delete_at_position(vm->read_only_frames, pos);

            PRINTLOG(HYPERVISOR, LOG_TRACE, "Section type: 0x%llx", res_md->section.section_type);
            PRINTLOG(HYPERVISOR, LOG_TRACE, "Section virtual start: 0x%llx", res_md->section.virtual_start);
            PRINTLOG(HYPERVISOR, LOG_TRACE, "Section size: 0x%llx", res_md->section.size);

            uint64_t section_size = res_md->section.size;
            uint64_t section_virtual_start = res_md->section.virtual_start;
            uint64_t section_physical_start = res_md->section.physical_start;
            uint64_t section_page_count = (section_size + MEMORY_PAGING_PAGE_LENGTH_4K - 1) / MEMORY_PAGING_PAGE_LENGTH_4K;

            for(uint64_t i = 0; i < section_page_count; i++) {
                if(hypervisor_ept_paging_del_page(vm, section_physical_start, section_virtual_start) != 0) {
                    PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to del readonly pages from page table");
                    return -1;
                }

                if(hypervisor_ept_del_ept_page(vm, section_physical_start, section_physical_start) != 0) {
                    PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to del readonly pages from ept");
                    return -1;
                }

                section_physical_start += MEMORY_PAGING_PAGE_LENGTH_4K;
                section_virtual_start += MEMORY_PAGING_PAGE_LENGTH_4K;
            }

            frame_t* new_section_frame = NULL;
            uint64_t frame_va = hypervisor_allocate_region(&new_section_frame, section_page_count * FRAME_SIZE);

            if(frame_va == -1ULL) {
                PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate frame for new section");
                return -1;
            }

            list_list_insert(vm->ept_frames, new_section_frame);

            uint64_t old_section_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(res_md->section.physical_start);

            memory_memcopy((void*)old_section_va, (void*)frame_va, section_size);

            uint64_t new_section_physical_start = new_section_frame->frame_address;
            section_virtual_start = res_md->section.virtual_start;
            section_physical_start = res_md->section.physical_start;

            for(uint64_t i = 0; i < section_page_count; i++) {
                if(hypervisor_ept_add_ept_page(vm, new_section_physical_start, section_physical_start, true) != 0) {
                    PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add new section pages to ept");
                    return -1;
                }

                if(hypervisor_ept_paging_add_page(vm, section_physical_start, section_virtual_start,
                                                  MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to add new section pages to page table");
                    return -1;
                }

                section_physical_start += MEMORY_PAGING_PAGE_LENGTH_4K;
                section_virtual_start += MEMORY_PAGING_PAGE_LENGTH_4K;
                new_section_physical_start += MEMORY_PAGING_PAGE_LENGTH_4K;
            }

            hypervisor_ept_invept(1);

            return (uint64_t)vmexit_info->registers;
        } else {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Write access to unknown memory at 0x%llx", error_address);
            return -1;
        }
    }

    if(!pagefault_error.fields.present) {
        uint64_t guest_rip = vmx_read(VMX_GUEST_RIP);
        uint64_t guest_rip_at_host = hypervisor_ept_guest_to_host(vm->ept_pml4_base, guest_rip);
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Page fault rip at host: 0x%llx", guest_rip_at_host);
        return -1;
    }

    return -1;
}

uint64_t hypervisor_ept_guest_virtual_to_host_physical(hypervisor_vm_t* vm, uint64_t guest_virtual) {
    uint64_t guest_physical = hypervisor_ept_paging_get_guest_physical(vm, guest_virtual);
    return hypervisor_ept_guest_to_host(vm->ept_pml4_base, guest_physical);
}
