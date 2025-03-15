/**
 * @file hypervisor_vmx_utils.h
 * @brief defines hypervisor related utility functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___HYPERVISOR_VMX_UTILS_H
#define ___HYPERVISOR_VMX_UTILS_H 0

#include <types.h>
#include <memory/paging.h>
#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_vmx_vmcs_ops.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint64_t (*vmx_vmexit_handler_t)(vmx_vmcs_vmexit_info_t* vmexit_info);

uint64_t hypervisor_allocate_region(frame_t** frame, uint64_t size);
uint64_t hypervisor_create_stack(hypervisor_vm_t* vm, uint64_t stack_size);
int8_t   hypevisor_deploy_program(hypervisor_vm_t* vm, const char_t* entry_point_name);
void     hypervisor_cleanup_mapped_interrupts(hypervisor_vm_t* vm);
int8_t   hypervisor_init_interrupt_mapped_vms(void);

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

int8_t   hypervisor_vmx_vmcall_load_module(hypervisor_vm_t* vm, vmx_vmcs_vmexit_info_t* vmexit_info);
uint64_t hypervisor_vmx_vmcall_attach_pci_dev(hypervisor_vm_t* vm, uint32_t pci_address);
int16_t  hypervisor_vmx_vmcall_attach_interrupt(hypervisor_vm_t* vm, vmx_vmcs_vmexit_info_t* vmexit_info);


#ifdef __cplusplus
}
#endif

#endif
