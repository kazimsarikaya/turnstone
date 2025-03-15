/**
 * @file hypervisor_vmx_vmcs_ops.h
 * @brief defines hypervisor related vmcs operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_VMX_VMCS_OPS_H
#define ___HYPERVISOR_VMX_VMCS_OPS_H 0

#include <types.h>
#include <hypervisor/hypervisor_vm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmx_vmcs_msr_blob_t {
    uint32_t index;
    uint32_t reserved;
    uint32_t msr_eax;
    uint32_t msr_edx;
} __attribute__((packed)) vmx_vmcs_msr_blob_t;

typedef struct vmx_vmcs_registers_t {
    uint64_t rflags;
    uint64_t cr2;
    uint8_t  sse[512];
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rsp;
    uint64_t rbp;
} __attribute__((packed)) vmx_vmcs_registers_t;

_Static_assert(sizeof(vmx_vmcs_registers_t) == 0x290, "vmx_vmcs_registers_t size mismatch");

typedef struct vmx_vmcs_vmexit_info {
    vmx_vmcs_registers_t* registers;
    uint64_t              guest_rflags;
    uint64_t              guest_rip;
    uint64_t              guest_rsp;
    uint64_t              guest_efer;
    uint64_t              guest_cr0;
    uint64_t              guest_cr3;
    uint64_t              guest_cr4;
    uint64_t              reason;
    uint64_t              exit_qualification;
    uint64_t              guest_linear_addr;
    uint64_t              guest_physical_addr;
    uint64_t              instruction_length;
    uint64_t              instruction_info;
    uint64_t              interrupt_info;
    uint64_t              interrupt_error_code;
    hypervisor_vm_t*      vm;
} vmx_vmcs_vmexit_info_t;

int8_t   hypervisor_vmx_vmcs_prepare_ept(hypervisor_vm_t* vm);
void     hypervisor_vmx_vmcs_dump(void);
uint32_t hypervisor_vmx_vmcs_revision_id(void);
int8_t   hypervisor_vmx_vmcs_prepare(hypervisor_vm_t** vm_out);
int8_t   hypervisor_vmx_vmcs_prepare_vmexit_handlers(void);

#ifdef __cplusplus
}
#endif

#endif
