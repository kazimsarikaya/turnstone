/**
 * @file hypervisor_utils.h
 * @brief defines hypervisor related utility functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___HYPERVISOR_UTILS_H
#define ___HYPERVISOR_UTILS_H 0

#include <types.h>
#include <memory/paging.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_vmcsops.h>


typedef uint64_t (*vmexit_handler_t)(vmcs_vmexit_info_t* vmexit_info);

uint64_t hypervisor_allocate_region(frame_t** frame, uint64_t size);
uint64_t hypervisor_create_stack(hypervisor_vm_t* vm, uint64_t stack_size);
int8_t   vmx_validate_capability(uint64_t target, uint32_t allowed0, uint32_t allowed1);
uint32_t vmx_fix_reserved_1_bits(uint32_t target, uint32_t allowed0);
uint32_t vmx_fix_reserved_0_bits(uint32_t target, uint32_t allowed1);

int8_t hypevisor_deploy_program(hypervisor_vm_t* vm, const char_t* entry_point_name);


void     hypervisor_vmcs_goto_next_instruction(vmcs_vmexit_info_t* vmexit_info);
uint64_t hypervisor_vmcs_vmcalls_handler(vmcs_vmexit_info_t* vmexit_info);
void     hypervisor_vmcall_cleanup_mapped_interrupts(hypervisor_vm_t* vm);

typedef struct hypervisor_vm_module_load_t {
    uint64_t old_got_physical_address;
    uint64_t old_got_size;
    uint64_t new_got_physical_address;
    uint64_t new_got_size;
    uint64_t module_physical_address;
    uint64_t module_virtual_address;
    uint64_t module_size;
    uint64_t metadata_physical_address;
    uint64_t metadata_virtual_address;
    uint64_t metadata_size;
} hypervisor_vm_module_load_t;

#endif
