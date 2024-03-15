/**
 * @file hypervisor_ept.h
 * @brief defines hypervisor related ept operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_EPT_H
#define ___HYPERVISOR_EPT_H 0

#include <types.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_utils.h>

typedef struct hypervisor_ept_pml4e_t {
    uint64_t read_access              :1;
    uint64_t write_access             :1;
    uint64_t execute_access           :1;
    uint64_t reserved0                :5;
    uint64_t accessed                 :1;
    uint64_t ignored1                 :1;
    uint64_t user_mode_execute_access :1;
    uint64_t ignored2                 :1;
    uint64_t address                  :40;
    uint64_t ignored3                 :12;
} __attribute__((packed)) hypervisor_ept_pml4e_t;

_Static_assert(sizeof(hypervisor_ept_pml4e_t) == 8, "PML4E size is not 8 bytes");

typedef struct hypervisor_ept_pdpte_t {
    uint64_t read_access              :1;
    uint64_t write_access             :1;
    uint64_t execute_access           :1;
    uint64_t reserved0                :5;
    uint64_t accessed                 :1;
    uint64_t ignored1                 :1;
    uint64_t user_mode_execute_access :1;
    uint64_t ignored2                 :1;
    uint64_t address                  :40;
    uint64_t ignored3                 :12;
} __attribute__((packed)) hypervisor_ept_pdpte_t;

_Static_assert(sizeof(hypervisor_ept_pdpte_t) == 8, "PDPTE size is not 8 bytes");

// pde is for 2mib pages
typedef struct hypervisor_ept_pde_2mib_t {
    uint64_t read_access              :1;
    uint64_t write_access             :1;
    uint64_t execute_access           :1;
    uint64_t memory_type              :3;
    uint64_t ignore_pat               :1;
    uint64_t must_one                 :1; // 2MIB pages field
    uint64_t accessed                 :1;
    uint64_t dirty                    :1;
    uint64_t user_mode_execute_access :1;
    uint64_t ignored1                 :1;
    uint64_t must_zero                :9;
    uint64_t address                  :31;
    uint64_t ignored2                 :8;
    uint64_t supervisor_shadow_stack  :1;
    uint64_t reserved3                :2;
    uint64_t suppress_ve              :1;
} __attribute__((packed)) hypervisor_ept_pde_2mib_t;

_Static_assert(sizeof(hypervisor_ept_pde_2mib_t) == 8, "PDE 2mib size is not 8 bytes");

typedef struct hypervisor_ept_pde_t {
    uint64_t read_access              :1;
    uint64_t write_access             :1;
    uint64_t execute_access           :1;
    uint64_t reserved0                :5;
    uint64_t accessed                 :1;
    uint64_t ignored1                 :1;
    uint64_t user_mode_execute_access :1;
    uint64_t ignored2                 :1;
    uint64_t address                  :40;
    uint64_t ignored3                 :12;
} __attribute__((packed)) hypervisor_ept_pde_t;

_Static_assert(sizeof(hypervisor_ept_pde_t) == 8, "PDE size is not 8 bytes");

// pde is for 2mib pages
typedef struct hypervisor_ept_pte_t {
    uint64_t read_access              :1;
    uint64_t write_access             :1;
    uint64_t execute_access           :1;
    uint64_t memory_type              :3;
    uint64_t ignore_pat               :1;
    uint64_t ignored0                 :1;
    uint64_t accessed                 :1;
    uint64_t dirty                    :1;
    uint64_t user_mode_execute_access :1;
    uint64_t ignored1                 :1;
    uint64_t address                  :40;
    uint64_t ignored2                 :8;
    uint64_t supervisor_shadow_stack  :1;
    uint64_t sub_page_write           :1;
    uint64_t reserved3                :1;
    uint64_t suppress_ve              :1;
} __attribute__((packed)) hypervisor_ept_pte_t;

_Static_assert(sizeof(hypervisor_ept_pte_t) == 8, "PTE size is not 8 bytes");

uint64_t hypervisor_ept_setup(hypervisor_vm_t* vm);
uint64_t hypervisor_ept_guest_to_host(uint64_t ept_base, uint64_t guest_physical);
int8_t   hypervisor_ept_build_tables(hypervisor_vm_t* vm);
int8_t   hypervisor_ept_merge_module(hypervisor_vm_t* vm, hypervisor_vm_module_load_t* module_load);

uint64_t hypervisor_ept_page_fault_handler(vmcs_vmexit_info_t* vmexit_info);

#endif
