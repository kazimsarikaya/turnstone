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
#include <cpu/descriptor.h>
#include <apic.h>
#include <strings.h>

MODULE("turnstone.hypervisor.guestlib");

void vm_guest_print(const char* str) {
    uint64_t str_len = strlen(str);

    asm volatile ("rep outsb"
                  :
                  : "S" (str), "c" (str_len), "d" (0x3f8)
                  : "memory");
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
                  : "a" (HYPERVISOR_VMCALL_NUMBER_EXIT)
                  : "memory");

    cpu_hlt();
}


uint64_t vm_guest_attach_pci_dev(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number) {
    uint64_t pci_address = (uint64_t)group_number << 24 |
                           (uint64_t)bus_number << 16 |
                           (uint64_t)device_number << 8 |
                           (uint64_t)function_number;

    uint64_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV), "D" (pci_address)
                  : "memory");

    return result;
}

uint64_t vm_guest_get_host_physical_address(uint64_t guest_virtual_address) {
    uint64_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS), "D" (guest_virtual_address)
                  : "memory");

    return result;
}

#define VMX_GUEST_IFEXT_BASE_VALUE 0x2000

vm_guest_interrupt_handler_t vm_guest_interrupt_handlers[256] = {0};

static __attribute__((interrupt, no_stack_protector, target("general-regs-only"), no_caller_saved_registers)) void vm_guest_interrupt_handler(void* notused) {
    UNUSED(notused);
    asm volatile ("cli");
    interrupt_frame_ext_t* frame = (interrupt_frame_ext_t*)VMX_GUEST_IFEXT_BASE_VALUE;
    vm_guest_interrupt_handler_t handler = vm_guest_interrupt_handlers[frame->interrupt_number];

    if(!handler) {
        vm_guest_printf("Unhandled interrupt: 0x%llx\n", frame->interrupt_number);
        vm_guest_exit();
    }

    handler(frame);
    asm volatile ("sti");
}

int16_t vm_guest_attach_interrupt(pci_generic_device_t* pci_dev, vm_guest_interrupt_type_t interrupt_type, uint8_t interrupt_number, vm_guest_interrupt_handler_t irq) {
    int16_t result = 0;

    asm volatile ("vmcall"
                  : "=a" (result)
                  : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT), "D" (pci_dev), "S" (interrupt_type), "d" (interrupt_number)
                  : "memory");

    if(result != -1) {
        descriptor_idt_t* idt = (descriptor_idt_t*)0x1000;

        DESCRIPTOR_BUILD_IDT_SEG(idt[result], (uint64_t)vm_guest_interrupt_handler, 0x08, 0x0, 0);
        vm_guest_interrupt_handlers[result] = irq;
    } else {
        vm_guest_printf("Failed to attach interrupt\n");
    }

    return result;
}

void vm_guest_apic_eoi(void) {
    cpu_write_msr(APIC_X2APIC_MSR_EOI, 0);
}
