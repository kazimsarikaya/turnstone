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

_Noreturn void vm_guest_exit(int32_t status) {
    uint64_t result = 0;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        asm volatile ("vmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_EXIT), "D" (status)
                      : "memory");
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        asm volatile ("vmmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_EXIT), "D" (status)
                      : "memory");
    }

    cpu_hlt();
}


uint64_t vm_guest_attach_pci_dev(uint8_t group_number, uint8_t bus_number, uint8_t device_number, uint8_t function_number) {
    uint64_t pci_address = (uint64_t)group_number << 24 |
                           (uint64_t)bus_number << 16 |
                           (uint64_t)device_number << 8 |
                           (uint64_t)function_number;

    uint64_t result = 0;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        asm volatile ("vmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV), "D" (pci_address)
                      : "memory");
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        asm volatile ("vmmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV), "D" (pci_address)
                      : "memory");
    }

    return result;
}

uint64_t vm_guest_get_host_physical_address(uint64_t guest_virtual_address) {
    uint64_t result = 0;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        asm volatile ("vmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS), "D" (guest_virtual_address)
                      : "memory");
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        asm volatile ("vmmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS), "D" (guest_virtual_address)
                      : "memory");
    }

    return result;
}

#define VMX_GUEST_IFEXT_BASE_VALUE 0x2000

vm_guest_interrupt_handler_t vm_guest_interrupt_handlers[256] = {0};

void vm_guest_generic_interrupt_handler(interrupt_frame_ext_t* frame);
void vm_guest_generic_interrupt_handler(interrupt_frame_ext_t* frame) {
    UNUSED(frame);
    cpu_cli();
    uint64_t* data = (uint64_t*)VMX_GUEST_IFEXT_BASE_VALUE;

    uint64_t vector = data[0];

    vm_guest_interrupt_handler_t handler = vm_guest_interrupt_handlers[vector];

    if(!handler) {
        vm_guest_printf("Unhandled interrupt 0x%llx\n", vector);
        vm_guest_exit(-vector + -0x1000);
    }

    handler(NULL);
}

static __attribute__((naked, no_stack_protector)) void vm_guest_interrupt_handler(void) {
    asm volatile (
        "push $0\n" // push error code
        "push $0\n" // push interrupt number
        "subq $0x2080, %rsp\n"
        "push %r15\n"
        "push %r14\n"
        "push %r13\n"
        "push %r12\n"
        "push %r11\n"
        "push %r10\n"
        "push %r9\n"
        "push %r8\n"
        "push %rbp\n"
        "push %rdi\n"
        "push %rsi\n"
        "push %rdx\n"
        "push %rcx\n"
        "push %rbx\n"
        "push %rax\n"
        "push %rsp\n"
        "mov %rsp, %rdi\n"
        "sub $0x8, %rsp\n"
        "lea 0x0(%rip), %rax\n"
        "movabsq $_GLOBAL_OFFSET_TABLE_, %rbx\n"
        "add %rax, %rbx\n"
        "movabsq $vm_guest_generic_interrupt_handler@GOT, %rax\n"
        "call *(%rbx, %rax, 1)\n"
        "add $0x8, %rsp\n"
        "pop %rsp\n"
        "pop %rax\n"
        "pop %rbx\n"
        "pop %rcx\n"
        "pop %rdx\n"
        "pop %rsi\n"
        "pop %rdi\n"
        "pop %rbp\n"
        "pop %r8\n"
        "pop %r9\n"
        "pop %r10\n"
        "pop %r11\n"
        "pop %r12\n"
        "pop %r13\n"
        "pop %r14\n"
        "pop %r15\n"
        "add $0x2090, %rsp\n"
        "iretq\n"
        );
}

int16_t vm_guest_attach_interrupt(pci_generic_device_t* pci_dev, vm_guest_interrupt_type_t interrupt_type, uint8_t interrupt_number, vm_guest_interrupt_handler_t irq) {
    int16_t result = 0;

    if(cpu_get_type() == CPU_TYPE_INTEL) {
        asm volatile ("vmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT), "D" (pci_dev), "S" (interrupt_type), "d" (interrupt_number)
                      : "memory");
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        asm volatile ("vmmcall"
                      : "=a" (result)
                      : "a" (HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT), "D" (pci_dev), "S" (interrupt_type), "d" (interrupt_number)
                      : "memory");
    }

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
