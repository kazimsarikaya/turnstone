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
#include <logging.h>

MODULE("turnstone.hypervisor");




uint64_t hypervisor_ept_setup(uint64_t low_mem, uint64_t high_mem) {
    if(low_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        low_mem += MEMORY_PAGING_PAGE_TYPE_2M - (low_mem % MEMORY_PAGING_PAGE_TYPE_2M);
    }

    if(high_mem % MEMORY_PAGING_PAGE_TYPE_2M != 0) {
        high_mem -= high_mem % MEMORY_PAGING_PAGE_TYPE_2M;
    }

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "low_mem: 0x%llx, high_mem: 0x%llx", low_mem, high_mem);


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

    frame_t* guest_frames = NULL;
    uint64_t guest_size = high_mem - low_mem;

    uint64_t guest_frames_va = hypervisor_allocate_region(&guest_frames, guest_size);
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "guest_frames_va: 0x%llx", guest_frames_va);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "ept_frames_va: 0x%llx", ept_frames_va);
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pde_row_count: 0x%llx pde_count: 0x%llx", pde_row_count, pde_count);
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pdpte_row_count: 0x%llx pdpte_count: 0x%llx", pdpte_row_count, pdpte_count);
    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pml4e_row_count: 0x%llx", pml4e_row_count);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_frames_va;

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)(ept_frames_va + FRAME_SIZE);
    hypervisor_ept_pdpte_t* pdptes_orig = pdptes;

    hypervisor_ept_pde_t* pdes = (hypervisor_ept_pde_t*)(ept_frames_va + FRAME_SIZE * (1 + pdpte_count));
    hypervisor_ept_pde_t* pdes_orig = pdes;

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
        pdes[i].address = (guest_frames->frame_address + (low_mem + i * MEMORY_PAGING_PAGE_LENGTH_2M)) >> 21;
    }

    return ept_frames->frame_address;
}

uint64_t hypervisor_ept_guest_to_host(uint64_t ept_base, uint64_t guest_physical) {
    ept_base = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ept_base);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "ept_base: 0x%llx, guest_physical: 0x%llx", ept_base, guest_physical);

    hypervisor_ept_pml4e_t* pml4e = (hypervisor_ept_pml4e_t*)ept_base;

    uint64_t pml4e_index = (guest_physical >> 39) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pml4e_index: 0x%llx", pml4e_index);

    if(pml4e[pml4e_index].read_access == 0 && pml4e[pml4e_index].write_access == 0 && pml4e[pml4e_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t pdpte_fa = pml4e[pml4e_index].address;
    pdpte_fa <<= 12;

    uint64_t pdpte_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pdpte_fa);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pdpte_va: 0x%llx", pdpte_va);

    hypervisor_ept_pdpte_t* pdptes = (hypervisor_ept_pdpte_t*)pdpte_va;

    uint64_t pdpte_index = (guest_physical >> 30) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pdpte_index: 0x%llx", pdpte_index);

    if(pdptes[pdpte_index].read_access == 0 && pdptes[pdpte_index].write_access == 0 && pdptes[pdpte_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t pde_fa = pdptes[pdpte_index].address;
    pde_fa <<= 12;

    uint64_t pde_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pde_fa);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pde_va: 0x%llx", pde_va);

    hypervisor_ept_pde_t* pdes = (hypervisor_ept_pde_t*)pde_va;

    uint64_t pde_index = (guest_physical >> 21) & 0x1FF;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "pde_index: 0x%llx", pde_index);

    if(pdes[pde_index].read_access == 0 && pdes[pde_index].write_access == 0 && pdes[pde_index].execute_access == 0) {
        return -1ULL;
    }

    uint64_t host_physical = pdes[pde_index].address;
    host_physical <<= 21;
    host_physical += guest_physical & ((1ULL << 21) - 1);

    return host_physical;
}
