/**
 * @file hypervisor_vmexit.64.c
 * @brief VMExit handler for 64-bit mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_vmcsops.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_vm.h>
#include <cpu.h>
#include <cpu/crx.h>
#include <cpu/interrupt.h>
#include <cpu/task.h>
#include <logging.h>
#include <list.h>
#include <apic.h>

MODULE("turnstone.hypervisor");

typedef uint64_t (*vmexit_handler_t)(vmcs_vmexit_info_t* vmexit_info);

vmexit_handler_t vmexit_handlers[VMX_VMEXIT_REASON_COUNT] = {0};

static void hypervisor_vmcs_goto_next_instruction(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t guest_rip = vmx_read(VMX_GUEST_RIP);
    guest_rip += vmexit_info->instruction_length;
    vmx_write(VMX_GUEST_RIP, guest_rip);
}

static uint64_t hypervisor_vmcs_external_interrupt_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t interrupt_info = vmexit_info->interrupt_info;
    uint64_t interrupt_vector = interrupt_info & 0xFF;
    uint64_t interrupt_type = (interrupt_info >> 8) & 0x7;
    uint64_t error_code = vmexit_info->interrupt_error_code;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "External Interrupt Type: 0x%llx", interrupt_type);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "External Interrupt Vector: 0x%llx", interrupt_vector);
    PRINTLOG(HYPERVISOR, LOG_TRACE, "External Interrupt Error Code: 0x%llx", error_code);

    cpu_cli();

    interrupt_frame_ext_t frame = {
        .return_rip = vmx_read(VMX_HOST_RIP),
        .return_cs = vmx_read(VMX_HOST_CS_SELECTOR),
        .return_rflags = vmexit_info->registers->rflags,
        .return_rsp = (uint64_t)vmexit_info->registers,
        .return_ss = vmx_read(VMX_HOST_SS_SELECTOR),
        .error_code = error_code,
        .interrupt_number = interrupt_vector,
        .rax = vmexit_info->registers->rax,
        .rbx = vmexit_info->registers->rbx,
        .rcx = vmexit_info->registers->rcx,
        .rdx = vmexit_info->registers->rdx,
        .rsi = vmexit_info->registers->rsi,
        .rdi = vmexit_info->registers->rdi,
        .rbp = vmexit_info->registers->rbp,
        .rsp = vmexit_info->registers->rsp,
        .r8 = vmexit_info->registers->r8,
        .r9 = vmexit_info->registers->r9,
        .r10 = vmexit_info->registers->r10,
        .r11 = vmexit_info->registers->r11,
        .r12 = vmexit_info->registers->r12,
        .r13 = vmexit_info->registers->r13,
        .r14 = vmexit_info->registers->r14,
        .r15 = vmexit_info->registers->r15,
    };

    interrupt_generic_handler(&frame);

    cpu_sti();

    hypervisor_vmcs_check_ipc(vmexit_info);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "External Interrupt Handler Returned: 0x%llx", (uint64_t)vmexit_info->registers);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_ept_misconfig_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t guest_linear_addr = vmexit_info->guest_linear_addr;
    uint64_t guest_physical_addr = vmexit_info->guest_physical_addr;
    uint64_t exit_qualification = vmexit_info->exit_qualification;
    uint64_t instruction_info = vmexit_info->instruction_info;
    uint64_t instruction_length = vmexit_info->instruction_length;

    uint64_t guest_rip = vmx_read(VMX_GUEST_RIP);
    uint64_t guest_cs = vmx_read(VMX_GUEST_CS_SELECTOR);
    uint64_t eptp = vmx_read(VMX_CTLS_EPTP);

    PRINTLOG(HYPERVISOR, LOG_ERROR, "EPT Misconfig: 0x%llx", exit_qualification);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Guest Linear Addr: 0x%llx", guest_linear_addr);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Guest Physical Addr: 0x%llx", guest_physical_addr);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Instruction Length: 0x%llx", instruction_length);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Instruction Info: 0x%llx", instruction_info);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Guest RIP: 0x%llx:0x%llx", guest_cs, guest_rip);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    EPTP: 0x%llx", eptp);

    return -1;
}

static uint64_t hypervisor_vmcs_hlt_handler(vmcs_vmexit_info_t* vmexit_info) {
    task_set_message_waiting();

    task_yield();

    hypervisor_vmcs_check_ipc(vmexit_info);

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_io_instruction_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t exit_qualification = vmexit_info->exit_qualification;

    uint16_t port = (exit_qualification >> 16) & 0xFFFF;
    uint8_t size = exit_qualification & 0x7;
    uint8_t direction = (exit_qualification >> 3) & 0x1;

    switch(size) {
    case 0:
        size = 1;
        break;
    case 1:
        size = 2;
        break;
    case 3:
        size = 4;
        break;
    default:
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Invalid IO Instruction Size: 0x%x", size);
        return -1;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Port: 0x%x, Size: 0x%x, Direction: 0x%x", port, size, direction)

    uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (64 - (size * 8));

    uint64_t data = vmexit_info->registers->rax;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Data: 0x%llx mask 0x%llx", data, mask);

    data &= mask;

    uint8_t* data_ptr = (uint8_t*)&data;

    if(port == 0x3f8 && direction == 0) {
        for(uint8_t i = 0; i < size; i++) {
            printf("%c", data_ptr[i]);
        }
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled IO Instruction: Port: 0x%x, Size: 0x%x, Direction: 0x%x Data 0x%llx",
                 port, size, direction, data);
        return -1;
    }

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_interrupt_window_handler(vmcs_vmexit_info_t* vmexit_info) {
    PRINTLOG(HYPERVISOR, LOG_TRACE, "Interrupt Window");

    uint32_t interuptibility_state = vmx_read(VMX_GUEST_INTERRUPTIBILITY_STATE);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "Interruptibility State: 0x%x rip: %llx", interuptibility_state, vmx_read(VMX_GUEST_RIP));


    hypervisor_vm_t* vm = task_get_vm();

    if(vm->need_to_notify) {
        uint32_t interrupt_info = 0;
        interrupt_info = vm->lapic.in_service_vector & 0xFF;
        interrupt_info |= (1 << 31); // valid

        if(vm->lapic.in_service_vector >= 0x20) {
            interrupt_info |= (0 << 8); // type

            vmx_write(VMX_CTLS_VM_ENTRY_EXCEPTION_ERROR_CODE, 0);

            PRINTLOG(HYPERVISOR, LOG_TRACE, "guest rip: 0x%llx", vmx_read(VMX_GUEST_RIP));
            PRINTLOG(HYPERVISOR, LOG_TRACE, "injected instruction length: %llx", vmexit_info->instruction_length);
            vmx_write(VMX_CTLS_VM_ENTRY_INSTRUCTION_LENGTH, vmexit_info->instruction_length);
        }

        vmx_write(VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD, interrupt_info);

        vm->lapic.apic_eoi_pending = true;
        vm->need_to_notify = false;

        if(vm->lapic_timer_pending) {
            vm->lapic_timer_pending = false;
        }

        PRINTLOG(HYPERVISOR, LOG_TRACE, "Interrupt Window: Injected Interrupt: 0x%x", interrupt_info);
    }

    if(!vm->lapic.apic_eoi_pending) {
        vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, vmx_read(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION) & ~(1 << 2));
    }

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_rdmsr_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t msr = vmexit_info->registers->rcx;

    hypervisor_vm_t* vm = task_get_vm();

    uint64_t value = 0;

    switch(msr) {
    case CPU_MSR_EFER:
        value = vmx_read(VMX_GUEST_IA32_EFER);
        break;
    case APIC_X2APIC_MSR_TIMER_INITIAL_VALUE:
        value = vm->lapic.timer_initial_value;
        break;
    case APIC_X2APIC_MSR_TIMER_CURRENT_VALUE:
        value = vm->lapic.timer_current_value;
        break;
    case APIC_X2APIC_MSR_TIMER_DIVIDER:
        value = vm->lapic.timer_divider;
        break;
    case APIC_X2APIC_MSR_LVT_TIMER:
        value = vm->lapic.timer_vector | (vm->lapic.timer_periodic << 17) | (vm->lapic.timer_masked << 16);
        break;
    default:
        value = (uint64_t)map_get(vm->msr_map, (void*)msr);
        break;
    }

    vmexit_info->registers->rax = value & 0xFFFFFFFF;
    vmexit_info->registers->rdx = (value >> 32) & 0xFFFFFFFF;

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_wrmsr_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t msr = vmexit_info->registers->rcx;
    uint64_t value = vmexit_info->registers->rax | (vmexit_info->registers->rdx << 32);

    hypervisor_vm_t* vm = task_get_vm();

    switch(msr) {
    case CPU_MSR_EFER:
        vmx_write(VMX_GUEST_IA32_EFER, value);
        break;
    case APIC_X2APIC_MSR_TIMER_INITIAL_VALUE:
        vm->lapic.timer_initial_value = value;
        vm->lapic.timer_current_value = value;
        vm->lapic_timer_enabled = true;
        break;
    case APIC_X2APIC_MSR_TIMER_CURRENT_VALUE:
        vm->lapic.timer_current_value = value;
        break;
    case APIC_X2APIC_MSR_TIMER_DIVIDER:
        vm->lapic.timer_divider = value;
        switch(value) {
        case 0x0:
            vm->lapic.timer_divider_realvalue = 2;
            break;
        case 0x1:
            vm->lapic.timer_divider_realvalue = 4;
            break;
        case 0x2:
            vm->lapic.timer_divider_realvalue = 8;
            break;
        case 0x3:
            vm->lapic.timer_divider_realvalue = 16;
            break;
        case 0x8:
            vm->lapic.timer_divider_realvalue = 32;
            break;
        case 0x9:
            vm->lapic.timer_divider_realvalue = 64;
            break;
        case 0xA:
            vm->lapic.timer_divider_realvalue = 128;
            break;
        case 0xB:
            vm->lapic.timer_divider_realvalue = 1;
            break;
        default:
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Invalid Timer Divider: 0x%llx", value);
            return -1;
        }

        break;
    case APIC_X2APIC_MSR_LVT_TIMER:
        vm->lapic.timer_vector = value & 0xFF;
        vm->lapic.timer_periodic = (value >> 17) & 0x1;
        vm->lapic.timer_masked = (value >> 16) & 0x1;
        break;
    case APIC_X2APIC_MSR_EOI:
        vm->lapic.apic_eoi_pending = false;
        PRINTLOG(HYPERVISOR, LOG_DEBUG, "EOI");
        return -1;
        break;
    default:
        map_insert(vm->msr_map, (void*)msr, (void*)value);
        break;
    }

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

uint64_t hypervisor_vmcs_exit_handler_entry(uint64_t rsp) {
    cpu_sti();

    vmcs_registers_t* registers = (vmcs_registers_t*)rsp;
    // registers->rsp = vmx_read(VMX_GUEST_RSP);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "VMExit RSP: 0x%llx", rsp);

    vmcs_vmexit_info_t vmexit_info = {
        .registers = registers,
        .reason = vmx_read(VMX_VMEXIT_REASON),
        .exit_qualification = vmx_read(VMX_EXIT_QUALIFICATION),
        .guest_linear_addr = vmx_read(VMX_GUEST_LINEAR_ADDR),
        .guest_physical_addr = vmx_read(VMX_GUEST_PHYSICAL_ADDR),
        .instruction_length = vmx_read(VMX_VMEXIT_INSTRUCTION_LENGTH),
        .instruction_info = vmx_read(VMX_VMEXIT_INSTRUCTION_INFO),
        .interrupt_info = vmx_read(VMX_VMEXIT_INTERRUPT_INFO),
        .interrupt_error_code = vmx_read(VMX_VMEXIT_INTERRUPT_ERROR_CODE),
    };

    if (vmexit_info.reason < VMX_VMEXIT_REASON_COUNT) {
        if (vmexit_handlers[vmexit_info.reason]) {
            return vmexit_handlers[vmexit_info.reason](&vmexit_info);
        }
    }

    // Unhandled VMExit
    PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled VMExit: 0x%llx", vmexit_info.reason);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Exit Qualification: 0x%llx", vmexit_info.exit_qualification);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Guest Linear Addr: 0x%llx", vmexit_info.guest_linear_addr);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Guest Physical Addr: 0x%llx", vmexit_info.guest_physical_addr);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Instruction Length: 0x%llx", vmexit_info.instruction_length);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Instruction Info: 0x%llx", vmexit_info.instruction_info);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Interrupt Info: 0x%llx", vmexit_info.interrupt_info);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    Interrupt Error Code: 0x%llx", vmexit_info.interrupt_error_code);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    RIP: 0x%llx", vmx_read(VMX_GUEST_RIP));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    RFLAGS: 0x%llx", vmx_read(VMX_GUEST_RFLAGS));
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    EFER: 0x%llx", vmx_read(VMX_GUEST_IA32_EFER));
    printf("rax: 0x%016llx ", vmexit_info.registers->rax);
    printf("rbx: 0x%016llx ", vmexit_info.registers->rbx);
    printf("rcx: 0x%016llx ", vmexit_info.registers->rcx);
    printf("rdx: 0x%016llx\n", vmexit_info.registers->rdx);
    printf("rsi: 0x%016llx ", vmexit_info.registers->rsi);
    printf("rdi: 0x%016llx ", vmexit_info.registers->rdi);
    printf("rbp: 0x%016llx ", vmexit_info.registers->rbp);
    printf("rsp: 0x%016llx\n", vmx_read(VMX_GUEST_RSP));
    printf("r8:  0x%016llx ", vmexit_info.registers->r8);
    printf("r9:  0x%016llx ", vmexit_info.registers->r9);
    printf("r10: 0x%016llx ", vmexit_info.registers->r10);
    printf("r11: 0x%016llx\n", vmexit_info.registers->r11);
    printf("r12: 0x%016llx ", vmexit_info.registers->r12);
    printf("r13: 0x%016llx ", vmexit_info.registers->r13);
    printf("r14: 0x%016llx ", vmexit_info.registers->r14);
    printf("r15: 0x%016llx\n", vmexit_info.registers->r15);
    printf("\n");

    printf("cr0: 0x%08llx cr2: 0x%016llx cr3: 0x%016llx cr4: 0x%08llx\n",
           vmx_read(VMX_GUEST_CR0), vmexit_info.registers->cr2, vmx_read(VMX_GUEST_CR3), vmx_read(VMX_GUEST_CR4));
    printf("\n");

    return -1;
}

int8_t hypervisor_vmcs_prepare_vmexit_handlers(void) {
    vmexit_handlers[VMX_VMEXIT_REASON_EXTERNAL_INTERRUPT] = hypervisor_vmcs_external_interrupt_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_EPT_MISCONFIG] = hypervisor_vmcs_ept_misconfig_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_HLT] = hypervisor_vmcs_hlt_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_IO_INSTRUCTION] = hypervisor_vmcs_io_instruction_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_INTERRUPT_WINDOW] = hypervisor_vmcs_interrupt_window_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_RDMSR] = hypervisor_vmcs_rdmsr_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_WRMSR] = hypervisor_vmcs_wrmsr_handler;
    return 0;
}
