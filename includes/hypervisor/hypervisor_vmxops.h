/**
 * @file hypervisor_vmxops.h
 * @brief vmx operations
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___HYPERVISOR_VMXOPS_H
#define ___HYPERVISOR_VMXOPS_H 0

#include <types.h>

int8_t   vmx_write(uint64_t field, uint64_t value);
uint64_t vmx_read(uint64_t field);
int8_t   vmxon(uint64_t vmxon_pa);
int8_t   vmptrld(uint64_t vmcs_pa);
int8_t   vmclear(uint64_t vmcs_pa);
int8_t   vmlaunch(void);
void     hypervisor_vmcs_dump(void);
#endif
