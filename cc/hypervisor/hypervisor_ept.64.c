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

MODULE("turnstone.hypervisor");




uint64_t hypervisor_ept_setup(hypervisor_vm_t* vm, uint64_t low_mem, uint64_t high_mem) {
    if(low_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        low_mem += MEMORY_PAGING_PAGE_TYPE_2M - (low_mem % MEMORY_PAGING_PAGE_TYPE_2M);
    }

    if(high_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        high_mem -= high_mem % MEMORY_PAGING_PAGE_TYPE_2M;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "low_mem: 0x%llx, high_mem: 0x%llx", low_mem, high_mem);


    uint64_t pde_row_count = (high_mem - low_mem) / MEMORY_PAGING_PAGE_LENGTH_2M;
    uint64_t pde_count = (pde_row_count + 512 - 1) / 512; // 512 PDEs per PDPTE entry
    uint64_t pdpte_row_count = (pde_row_count + 512 - 1) / 512; // 512 PDEs per PDPT entry
    uint64_t pdpte_count = (pdpte_row_count + 512 - 1) / 512; // 512 PDPTs per PML4T entry
    uint64_t pml4e_row_count = (pdpte_row_count + 512 - 1) / 512; // 512 PDPTs per PML4T entry

    uint64_t total_count = 1 + pdpte_count + pde_count; // PML4T + PDPTs + PDEs

    frame_t* ept_frames = NULL;
    uint64_t ept_frames_va = hypervisor_allocate_region(&ept_frames, total_count * FRAME_SIZE);

    if(ept_frames_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate EPT frames");
        return 0;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_EPT] = *ept_frames;

    frame_t* guest_frames = NULL;
    uint64_t guest_size = high_mem - low_mem;

    uint64_t guest_frames_va = hypervisor_allocate_region(&guest_frames, guest_size);

    if(guest_frames_va == 0) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Failed to allocate guest frames");
        return 0;
    }

    vm->owned_frames[HYPERVISOR_VM_FRAME_TYPE_GUEST] = *guest_frames;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "guest_frames_va: 0x%llx", guest_frames_va);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "ept_frames_va: 0x%llx", ept_frames_va);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "pde_row_count: 0x%llx pde_count: 0x%llx", pde_row_count, pde_count);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "pdpte_row_count: 0x%llx pdpte_count: 0x%llx", pdpte_row_count, pdpte_count);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "pml4e_row_count: 0x%llx", pml4e_row_count);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_frames_va;

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)(ept_frames_va + FRAME_SIZE);
    hypervisor_ept_pdpte_t* pdptes_orig = pdptes;

    hypervisor_ept_pde_2mib_t* pdes = (hypervisor_ept_pde_2mib_t*)(ept_frames_va + FRAME_SIZE * (1 + pdpte_count));
    hypervisor_ept_pde_2mib_t* pdes_orig = pdes;

    for(uint64_t i = 0; i < pml4e_row_count; i++) {
        pml4e[i].read_access = 1;
        pml4e[i].write_access = 1;
        pml4e[i].execute_access = 1;
        pml4e[i].user_mode_execute_access = 1;
        uint64_t pdpte_va = (uint64_t)pdptes;
        uint64_t pdpte_pa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(pdpte_va);
        pml4e[i].address = pdpte_pa >> 12;
        pdptes += 512;
    }

    pdptes = pdptes_orig;

    for(uint64_t i = 0; i < pdpte_row_count; i++) {
        pdptes[i].read_access = 1;
        pdptes[i].write_access = 1;
        pdptes[i].execute_access = 1;
        pdptes[i].user_mode_execute_access = 1;
        uint64_t pde_va = (uint64_t)pdes;
        uint64_t pde_pa = MEMORY_PAGING_GET_FA_FOR_RESERVED_VA(pde_va);
        pdptes[i].address = pde_pa >> 12;
        pdes += 512;
    }

    pdes = pdes_orig;

    for(uint64_t i = 0; i < pde_row_count; i++) {
        pdes[i].read_access = 1;
        pdes[i].write_access = 1;
        pdes[i].execute_access = 1;
        pdes[i].user_mode_execute_access = 1;
        pdes[i].memory_type = 6; // WB
        pdes[i].ignore_pat = 1;
        pdes[i].must_one = 1; // 2MB page
        pdes[i].address = (guest_frames->frame_address + (low_mem + i * MEMORY_PAGING_PAGE_LENGTH_2M)) >> 21;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "EPT setup complete. EPT frames VA: 0x%llx", ept_frames_va);

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

int8_t hypervisor_ept_build_tables(uint64_t ept_base, uint64_t low_mem, uint64_t high_mem) {
    // FIXME: now only supports low_mem is 0

    if(low_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        low_mem += MEMORY_PAGING_PAGE_TYPE_2M - (low_mem % MEMORY_PAGING_PAGE_TYPE_2M);
    }

    if(high_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        high_mem -= high_mem % MEMORY_PAGING_PAGE_TYPE_2M;
    }

    uint64_t guest_low = hypervisor_ept_guest_to_host(ept_base, low_mem);

    if(guest_low == -1ULL) {
        return -1;
    }

    guest_low = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(guest_low);

    uint64_t* gdt = (uint64_t*)(guest_low + 0x2000);

    gdt[0] = 0;
    gdt[1] = 0x00209b0000000000ULL;
    gdt[2] = 0x0000930000000000ULL;
    gdt[3] = 0x00008b0030000067ULL;

#if 0
    uint64_t l3_row_count = (high_mem - low_mem) / MEMORY_PAGING_PAGE_LENGTH_2M;
    // uint64_t l3_count = (l3_row_count + 512 - 1) / 512; // 512 PDEs per PDPTE entry
    uint64_t l2_row_count = (l3_row_count + 512 - 1) / 512; // 512 PDEs per PDPT entry
    uint64_t l2_count = (l2_row_count + 512 - 1) / 512; // 512 PDPTs per PML4T entry
    uint64_t l1_row_count = (l2_row_count + 512 - 1) / 512; // 512 PDPTs per PML4T entry


    memory_page_table_t* l1 = (memory_page_table_t*)(guest_low + 0x4000);
    uint64_t l2_fa = 0x5000;
    uint64_t orig_l2_fa = l2_fa;

    for(uint64_t i = 0; i < l1_row_count; i++) {
        l1->pages[i].present = 1;
        l1->pages[i].writable = 1;
        l1->pages[i].physical_address = l2_fa >> 12;
        l2_fa += 0x1000;
    }

    memory_page_table_t* l2 = l1 + 1;
    l2_fa = orig_l2_fa;
    uint64_t l3_fa = l2_fa + 0x1000 * l2_count;

    for(uint64_t i = 0; i < l2_row_count; i++) {
        l2->pages[i].present = 1;
        l2->pages[i].writable = 1;
        l2->pages[i].physical_address = l3_fa >> 12;
        l3_fa += 0x1000;
    }

    memory_page_entry_t* l3 = (memory_page_entry_t*)(l2 + l2_count);

    for(uint64_t i = 0; i < l3_row_count; i++) {
        l3[i].present = 1;
        l3[i].writable = 1;
        l3[i].hugepage = 1;
        l3[i].physical_address = (0 + i * MEMORY_PAGING_PAGE_LENGTH_2M) >> 12;
    }
#else
    // for ept testing create identity map for first 4GB with 1GB pages
    memory_page_table_t* l1 = (memory_page_table_t*)(guest_low + 0x4000);
    uint64_t l2_fa = 0x5000;

    l1->pages[0].present = 1;
    l1->pages[0].writable = 1;
    l1->pages[0].physical_address = l2_fa >> 12;

    memory_page_entry_t* l2 = (memory_page_entry_t*)(l1 + 1);

    for(uint64_t i = 0; i < 4; i++) {
        l2[i].present = 1;
        l2[i].writable = 1;
        l2[i].hugepage = 1;
        l2[i].physical_address = (0 + i * MEMORY_PAGING_PAGE_LENGTH_1G) >> 12;
    }
#endif
    return 0;
}
