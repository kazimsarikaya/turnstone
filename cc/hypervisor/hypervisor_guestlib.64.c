/**
 * @file hypervisor_guestlib.64.c
 * @brief Hypervisor Guest Library
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */



#include <hypervisor/hypervisor_guestlib.h>
#include <ports.h>
#include <buffer.h>
#include <cpu.h>

MODULE("turnstone.hypervisor.guestlib");

void vm_guest_print(const char* str) {
    while(*str != '\0') {
        outb(0x3f8, *str);
        str++;
    }
}

void __attribute__((format(printf, 1, 2))) vm_guest_printf(const char* fstr, ...){
    buffer_t* buffer = buffer_new();

    va_list args;
    va_start(args, fstr);

    buffer_vprintf(buffer, fstr, args);

    va_end(args);

    const uint8_t* out = buffer_get_all_bytes_and_destroy(buffer, NULL);

    vm_guest_print((const char*)out);

}

_Noreturn void vm_guest_halt(void) {
    cpu_hlt();
}

_Noreturn void vm_guest_exit(void) {
    uint64_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_EXIT)
                  : "memory");

    cpu_hlt();
}


uint64_t vm_guest_attach_pci_dev(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number) {
    uint64_t pci_address = (uint64_t)group_number << 24 |
                           (uint64_t)bus_number << 16 |
                           (uint64_t)device_number << 11 |
                           (uint64_t)function_number << 8;

    uint64_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_ATTACH_PCI_DEV), "D" (pci_address)
                  : "memory");

    return result;
}

uint64_t vm_guest_get_host_physical_address(uint64_t guest_virtual_address) {
    uint64_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_GET_HOST_PHYSICAL_ADDRESS), "D" (guest_virtual_address)
                  : "memory");

    return result;
}
