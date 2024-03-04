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

typedef struct vmcs_msr_blob_t {
    uint32_t index;
    uint32_t reserved;
    uint32_t msr_eax;
    uint32_t msr_edx;
} __attribute__((packed)) vmcs_msr_blob_t;

typedef struct vmcs_registers_t {
    uint64_t rflags;
    uint64_t cr2;
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

_Static_assert(sizeof(vmcs_registers_t) == 0x90, "vmcs_registers_t size mismatch");


uint32_t hypervisor_vmcs_revision_id(void);
int8_t   hypervisor_vmcs_prepare_host_state(void);
int8_t   hypervisor_vmcs_prepare_guest_state(void);
int8_t   hypervisor_vmcs_prepare_pinbased_control(void);
int8_t   hypervisor_vmcs_prepare_procbased_control(void);
void     hypervisor_io_bitmap_set_port(uint8_t * bitmap, uint16_t port);
int8_t   hypervisor_vmcs_prepare_io_bitmap(void);
int8_t   hypervisor_vmcs_prepare_execution_control(void);
int8_t   hypervisor_vmcs_prepare_vm_exit_and_entry_control(void);
int8_t   hypervisor_vmcs_prepare_ept(void);
int8_t   hypervisor_vmcs_prepare_vmexit_handlers(void);
uint64_t hypervisor_vmcs_exit_handler_entry(uint64_t rsp);
#endif

