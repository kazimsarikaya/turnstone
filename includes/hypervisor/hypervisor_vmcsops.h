/**
 * @file hypervisor_vmcsops.h
 * @brief defines hypervisor related vmcs operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___HYPERVISOR_VMCSOPS_H
#define ___HYPERVISOR_VMCSOPS_H 0

#include <types.h>
#include <hypervisor/hypervisor_vm.h>

typedef struct vmcs_msr_blob_t {
    uint32_t index;
    uint32_t reserved;
    uint32_t msr_eax;
    uint32_t msr_edx;
} __attribute__((packed)) vmcs_msr_blob_t;

typedef struct vmcs_registers_t {
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
} __attribute__((packed)) vmcs_registers_t;

_Static_assert(sizeof(vmcs_registers_t) == 0x290, "vmcs_registers_t size mismatch");

typedef struct vmcs_vmexit_info {
    vmcs_registers_t* registers;
    uint64_t          guest_rflags;
    uint64_t          guest_rip;
    uint64_t          guest_rsp;
    uint64_t          guest_efer;
    uint64_t          guest_cr0;
    uint64_t          guest_cr3;
    uint64_t          guest_cr4;
    uint64_t          reason;
    uint64_t          exit_qualification;
    uint64_t          guest_linear_addr;
    uint64_t          guest_physical_addr;
    uint64_t          instruction_length;
    uint64_t          instruction_info;
    uint64_t          interrupt_info;
    uint64_t          interrupt_error_code;
    hypervisor_vm_t*  vm;
}vmcs_vmexit_info_t;

uint32_t hypervisor_vmcs_revision_id(void);
int8_t   hypervisor_vmcs_prepare_host_state(hypervisor_vm_t* vm);
int8_t   hypervisor_vmcs_prepare_guest_state(void);
int8_t   hypervisor_vmcs_prepare_pinbased_control(void);
int8_t   hypervisor_vmcs_prepare_procbased_control(hypervisor_vm_t* vm);
int8_t   hypervisor_msr_bitmap_set(uint8_t * bitmap, uint32_t msr, boolean_t read);
void     hypervisor_io_bitmap_set_port(uint8_t * bitmap, uint16_t port);
int8_t   hypervisor_vmcs_prepare_io_bitmap(hypervisor_vm_t* vm);
int8_t   hypervisor_vmcs_prepare_execution_control(hypervisor_vm_t* vm);
int8_t   hypervisor_vmcs_prepare_vm_exit_and_entry_control(hypervisor_vm_t* vm);
int8_t   hypervisor_vmcs_prepare_ept(hypervisor_vm_t* vm);
int8_t   hypervisor_vmcs_prepare_vmexit_handlers(void);
uint64_t hypervisor_vmcs_exit_handler_entry(uint64_t rsp);
#endif

