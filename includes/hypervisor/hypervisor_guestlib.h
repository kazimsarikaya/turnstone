/**
 * @file hypervisor_guestlib.h
 * @brief Hypervisor Guest Library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#ifndef ___HYPERVISOR_GUESTLIB_H
#define ___HYPERVISOR_GUESTLIB_H 0

#include <types.h>

#define HYPERVISOR_VMCALL_EXIT                        0x0
#define HYPERVISOR_VMCALL_GET_HOST_PHYSICAL_ADDRESS 0x100
#define HYPERVISOR_VMCALL_ATTACH_PCI_DEV            0x200
#define HYPERVISOR_VMCALL_LOAD_MODULE              0x1000

void                                       vm_guest_print(const char* str);
void __attribute__((format(printf, 1, 2))) vm_guest_printf(const char* fstr, ...);
_Noreturn void                             vm_guest_halt(void);
uint64_t                                   vm_guest_attach_pci_dev(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number);
uint64_t                                   vm_guest_get_host_physical_address(uint64_t guest_virtual_address);
_Noreturn void                             vm_guest_exit(void);

#endif // ___HYPERVISOR_GUESTLIB_H
