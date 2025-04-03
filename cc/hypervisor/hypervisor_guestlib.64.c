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

static boolean_t vm_guest_interrupt_xsave_mask_memorized = false;
static uint64_t vm_guest_interrupt_xsave_mask_lo = 0;
static uint64_t vm_guest_interrupt_xsave_mask_hi = 0;

static void vm_guest_interrupt_save_restore_avx512f(boolean_t save, interrupt_frame_ext_t* frame) {
    if(!vm_guest_interrupt_xsave_mask_memorized) {
        cpu_cpuid_regs_t query = {0};
        cpu_cpuid_regs_t result;

        query.eax = 0xd;

        cpu_cpuid(query, &result);

        vm_guest_interrupt_xsave_mask_lo = result.eax;
        vm_guest_interrupt_xsave_mask_hi = result.edx;

        vm_guest_interrupt_xsave_mask_memorized = true;
    }

    uint64_t frame_base = (uint64_t)frame;
    uint64_t avx512f_offset = frame_base + offsetof_field(interrupt_frame_ext_t, avx512f);
    // align to 0x40
    avx512f_offset = (avx512f_offset + 0x3F) & ~0x3F;

    if(save) {
        memory_memclean((void*)avx512f_offset, 0x2000);

        asm volatile (
            "mov %[avx512f_offset], %%rbx\n"
            "xsave (%%rbx)\n"
            :
            :
            [avx512f_offset] "r" (avx512f_offset),
            "rax" (vm_guest_interrupt_xsave_mask_lo),
            "rdx" (vm_guest_interrupt_xsave_mask_hi)
            : "rbx"
            );
    } else {
        asm volatile (
            "mov %[avx512f_offset], %%rbx\n"
            "xrstor (%%rbx)\n"
            :
            :
            [avx512f_offset] "r" (avx512f_offset),
            "rax" (vm_guest_interrupt_xsave_mask_lo),
            "rdx" (vm_guest_interrupt_xsave_mask_hi)
            : "rbx"
            );
    }
}

void vm_guest_interrupt_generic_handler(interrupt_frame_ext_t* frame);
void vm_guest_interrupt_generic_handler(interrupt_frame_ext_t* frame) {
    vm_guest_interrupt_save_restore_avx512f(true, frame);

    uint64_t vector = frame->interrupt_number;

    vm_guest_interrupt_handler_t handler = vm_guest_interrupt_handlers[vector];

    if(!handler) {
        vm_guest_printf("Unhandled interrupt 0x%llx\n", vector);
        vm_guest_exit(-vector + -0x1000);
    }

    handler(NULL);

    vm_guest_interrupt_save_restore_avx512f(false, frame);
}

static boolean_t vm_guest_interrupt_dummy_handlers_registered = false;
void vm_guest_interrupt_register_dummy_handlers(descriptor_idt_t*);

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
        if(!vm_guest_interrupt_dummy_handlers_registered) {

            descriptor_idt_t* idt = (descriptor_idt_t*)0x1000;

            vm_guest_interrupt_register_dummy_handlers(idt);

            vm_guest_interrupt_dummy_handlers_registered = true;
        }

        vm_guest_interrupt_handlers[result] = irq;
    } else {
        vm_guest_printf("Failed to attach interrupt\n");
    }

    return result;
}

void vm_guest_apic_eoi(void) {
    cpu_write_msr(APIC_X2APIC_MSR_EOI, 0);
}

void vm_guest_enable_timer(vm_guest_interrupt_handler_t handler, uint32_t initial_value, uint32_t divider) {
    if(!vm_guest_interrupt_dummy_handlers_registered) {

        descriptor_idt_t* idt = (descriptor_idt_t*)0x1000;

        vm_guest_interrupt_register_dummy_handlers(idt);

        vm_guest_interrupt_dummy_handlers_registered = true;
    }

    cpu_write_msr(APIC_X2APIC_MSR_TIMER_INITIAL_VALUE, initial_value);
    cpu_write_msr(APIC_X2APIC_MSR_TIMER_DIVIDER, divider);
    cpu_write_msr(APIC_X2APIC_MSR_LVT_TIMER, APIC_TIMER_PERIODIC | APIC_INTERRUPT_ENABLED | 0x20);
    vm_guest_interrupt_handlers[0x20] = handler;
}
