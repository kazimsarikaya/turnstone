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
#include <cpu/interrupt.h>
#include <pci.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hypervisor_vmcall_number_t {
    HYPERVISOR_VMCALL_NUMBER_EXIT                     =    0x0,
    HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS=  0x100,
    HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV           =  0x200,
    HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT         =  0x201,
    HYPERVISOR_VMCALL_NUMBER_LOAD_MODULE              = 0x1000,
} hypervisor_vmcall_number_type_t;

typedef enum vm_guest_interrupt_type_t {
    VM_GUEST_INTERRUPT_TYPE_LEGACY = 0x0,
    VM_GUEST_INTERRUPT_TYPE_MSI    = 0x1,
    VM_GUEST_INTERRUPT_TYPE_MSIX   = 0x2,
} vm_guest_interrupt_type_t;

typedef void (*vm_guest_interrupt_handler_t)(interrupt_frame_ext_t* frame);

void                                       vm_guest_print(const char* str);
void __attribute__((format(printf, 1, 2))) vm_guest_printf(const char* fstr, ...);
_Noreturn void                             vm_guest_halt(void);
uint64_t                                   vm_guest_attach_pci_dev(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number);
int16_t                                    vm_guest_attach_interrupt(pci_generic_device_t* pci_dev, vm_guest_interrupt_type_t interrupt_type, uint8_t interrupt_number, vm_guest_interrupt_handler_t irq);
uint64_t                                   vm_guest_get_host_physical_address(uint64_t guest_virtual_address);
_Noreturn void                             vm_guest_exit(int32_t status);
void                                       vm_guest_apic_eoi(void);

#ifdef __cplusplus
}
#endif

#endif // ___HYPERVISOR_GUESTLIB_H
