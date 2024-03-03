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

int8_t hypervisor_vmcs_prepare_host_state(void);
int8_t hypervisor_vmcs_prepare_guest_state(void);
int8_t hypervisor_vmcs_prepare_pinbased_control(void);
int8_t hypervisor_vmcs_prepare_procbased_control(void);
void   hypervisor_io_bitmap_set_port(uint8_t * bitmap, uint16_t port);
int8_t hypervisor_vmcs_prepare_io_bitmap(void);
int8_t hypervisor_vmcs_prepare_execution_control(void);
int8_t hypervisor_vmcs_prepare_vm_exit_and_entry_control(void);
int8_t hypervisor_vmcs_prepare_ept(void);

#endif

