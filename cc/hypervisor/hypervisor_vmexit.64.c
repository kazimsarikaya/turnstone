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
#include <ports.h>
#include <strings.h>

MODULE("turnstone.hypervisor");

vmexit_handler_t vmexit_handlers[VMX_VMEXIT_REASON_COUNT] = {0};

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

    uint64_t guest_rip = vmexit_info->guest_rip;
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
    hypervisor_vm_t* vm = vmexit_info->vm;
    vm->is_halted = true;

    task_set_message_waiting();

    task_yield();

    hypervisor_vmcs_check_ipc(vmexit_info);

    if(vm->is_halt_need_next_instruction) {
        vm->is_halted = false;
        vm->is_halt_need_next_instruction = false;
        hypervisor_vmcs_goto_next_instruction(vmexit_info);
    }

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_pause_handler(vmcs_vmexit_info_t* vmexit_info) {
    hypervisor_vm_t* vm = vmexit_info->vm;
    vm->is_halted = true;

    task_yield();

    hypervisor_vmcs_check_ipc(vmexit_info);

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_io_instruction_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t exit_qualification = vmexit_info->exit_qualification;
    hypervisor_vm_t* vm = vmexit_info->vm;

    uint64_t port = (exit_qualification >> 16) & 0xFFFF;
    uint8_t size = exit_qualification & 0x7;
    uint8_t direction = (exit_qualification >> 3) & 0x1;
    boolean_t is_string = (exit_qualification >> 4) & 0x1;
    boolean_t is_rep = (exit_qualification >> 5) & 0x1;

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

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Port: 0x%llx, Size: 0x%x, Direction: 0x%x String %i Rep %i",
             port, size, direction, is_string, is_rep);

    uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (64 - (size * 8));

    uint64_t data = vmexit_info->registers->rax;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Data: 0x%llx mask 0x%llx", data, mask);

    data &= mask;

    // TODO: handle direction flag, also handle rep at mapped io ports when rep is set update rsi and rcx

    if(list_contains(vm->mapped_io_ports, (void*)port) == 0){
        if(direction == 0) {
            if(size == 1) {
                outb(port, data);
            } else if(size == 2) {
                outw(port, data);
            } else {
                outl(port, data);
            }
        } else {
            uint64_t value = 0;

            if(size == 1) {
                value = inb(port);
            } else if(size == 2) {
                value = inw(port);
            } else {
                value = inl(port);
            }

            vmexit_info->registers->rax = (vmexit_info->registers->rax & ~mask) | (value & mask);
        }
    } else {

        if(port == 0x3f8 && direction == 0) {
            if(is_string && is_rep) {
                uint64_t rsi = vmexit_info->registers->rsi;
                uint64_t rcx = vmexit_info->registers->rcx;

                uint64_t data_ptr_fa = hypervisor_ept_guest_virtual_to_host_physical(vm, rsi);
                uint64_t data_ptr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_ptr_fa);

                PRINTLOG(HYPERVISOR, LOG_TRACE,
                         "IO Instruction String: port 0x%llx size: 0x%llx, rsi 0x%llx, data ptr fa 0x%llx va 0x%llx",
                         port, rcx, rsi, data_ptr_fa, data_ptr_va);

                char_t* data_ptr = (char_t*)data_ptr_va;

                uint64_t data_len = strlen(data_ptr);

                if(rcx != data_len) {
                    PRINTLOG(HYPERVISOR, LOG_ERROR, "IO Instruction String Length Mismatch: 0x%llx 0x%llx", rcx, data_len);
                }

                char_t tmp = data_ptr[data_len];
                data_ptr[data_len] = 0;

                printf("%s", data_ptr);

                data_ptr[data_len] = tmp;

                vmexit_info->registers->rsi += data_len;
                vmexit_info->registers->rcx -= data_len;
            } else {
                uint8_t* data_ptr = (uint8_t*)&data;
                for(uint8_t i = 0; i < size; i++) {
                    printf("%c", data_ptr[i]);
                }
            }
        } else {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled IO Instruction: Port: 0x%llx, Size: 0x%x, Direction: 0x%x Data 0x%llx",
                     port, size, direction, data);
            return -1;
        }
    }

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static void hypervisor_vmcs_find_next_x2apic_interrupt(vmcs_vmexit_info_t* vmexit_info, hypervisor_vm_t* vm, boolean_t iterate, boolean_t for_eoi) {
    uint32_t interrupt_vector = 0;
    boolean_t found = false;

    if(for_eoi && vm->lapic.in_service_vector == vm->lapic.timer_vector && vm->lapic_timer_pending) {
        vm->lapic_timer_pending = false;
    }

    uint64_t waiting_int_count = 0;

    for(uint32_t vector = 0; vector < 256; vector++) {
        uint32_t vector_byte = vector / 64;
        uint32_t vector_bit = vector % 64;

        if(!found && vm->lapic.in_request_vectors[vector_byte] & (1 << vector_bit)) {
            interrupt_vector = vector;
            found = true;

            if(iterate) {
                vm->lapic.in_request_vectors[vector_byte] &= ~(1 << vector_bit);
            }
        }

        uint64_t popcnt = 0;
        asm volatile ("popcnt %1, %0" : "=r" (popcnt) : "r" (vm->lapic.in_request_vectors[vector_byte]));

        waiting_int_count += popcnt;
    }


    if(found) {
        if(iterate) {
            vm->lapic.in_service_vector = interrupt_vector;
            vm->lapic.apic_eoi_pending = true;

            if(interrupt_vector != 0x20 && list_size(vm->interrupt_queue)){
                interrupt_frame_ext_t* frame = (interrupt_frame_ext_t*)list_queue_pop(vm->interrupt_queue);

                uint64_t guest_ifext_fa = hypervisor_ept_guest_to_host(vm->ept_pml4_base, VMX_GUEST_IFEXT_BASE_VALUE);
                uint64_t guest_ifext_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(guest_ifext_fa);

                memory_memcopy(frame, (void*)guest_ifext_va, sizeof(interrupt_frame_ext_t));

                memory_free_ext(vm->heap, frame);
            }
        }

        uint64_t rflags = vmexit_info->guest_rflags;

        if((rflags & (1 << 9)) && waiting_int_count) {
            vm->need_to_notify = true;
        }

    } else {
        vm->lapic.in_service_vector = 0;
        vm->need_to_notify = false;
        vm->lapic.apic_eoi_pending = false;
    }

}

static uint64_t hypervisor_vmcs_interrupt_window_handler(vmcs_vmexit_info_t* vmexit_info) {
    PRINTLOG(HYPERVISOR, LOG_TRACE, "Interrupt Window");

    uint32_t interuptibility_state = vmx_read(VMX_GUEST_INTERRUPTIBILITY_STATE);

    PRINTLOG(HYPERVISOR, LOG_TRACE, "Interruptibility State: 0x%x rip: %llx", interuptibility_state, vmexit_info->guest_rip);


    hypervisor_vm_t* vm = vmexit_info->vm;

    if(vm->need_to_notify) {
        hypervisor_vmcs_find_next_x2apic_interrupt(vmexit_info, vm, true, false);

        if(vm->need_to_notify) { // if there is an interrupt need_to_notify still true
            uint32_t interrupt_info = 0;
            interrupt_info = vm->lapic.in_service_vector & 0xFF;
            interrupt_info |= (1 << 31); // valid

            if(vm->lapic.in_service_vector >= 0x20) {
                interrupt_info |= (0 << 8); // type

                vmx_write(VMX_CTLS_VM_ENTRY_EXCEPTION_ERROR_CODE, 0);

                PRINTLOG(HYPERVISOR, LOG_TRACE, "guest rip: 0x%llx", vmexit_info->guest_rip);
                PRINTLOG(HYPERVISOR, LOG_TRACE, "injected instruction length: %llx", vmexit_info->instruction_length);
                vmx_write(VMX_CTLS_VM_ENTRY_INSTRUCTION_LENGTH, vmexit_info->instruction_length);
            }

            vmx_write(VMX_CTLS_VM_ENTRY_INTERRUPT_INFORMATION_FIELD, interrupt_info);

            PRINTLOG(HYPERVISOR, LOG_TRACE, "Interrupt Window: Injected Interrupt: 0x%x", interrupt_info);

            vm->need_to_notify = false;
        }
    }

    if(!vm->need_to_notify) {
        vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, vmx_read(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION) & ~(1 << 2));
    }

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_rdmsr_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t msr = vmexit_info->registers->rcx;

    hypervisor_vm_t* vm = vmexit_info->vm;

    uint64_t value = 0;

    switch(msr) {
    case CPU_MSR_EFER:
        value = vmexit_info->guest_efer;
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

    hypervisor_vm_t* vm = vmexit_info->vm;

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
        hypervisor_vmcs_find_next_x2apic_interrupt(vmexit_info, vm, false, true);
        vm->lapic.apic_eoi_pending = false;
        break;
    default:
        map_insert(vm->msr_map, (void*)msr, (void*)value);
        break;
    }

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}
static uint64_t hypervisor_vmcs_control_register_access_handler(vmcs_vmexit_info_t* vmexit_info) {
    uint64_t exit_qualification = vmexit_info->exit_qualification;
    uint64_t reg = (exit_qualification >> 8) & 0xF;
    uint64_t access_type = (exit_qualification >> 4) & 0x3;
    uint64_t cr = exit_qualification & 0xF;

    if(reg != 15) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled Control Register Access: 0x%llx", exit_qualification);
        return -1;
    }

    if(cr != 3) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled Control Register Access: 0x%llx", exit_qualification);
        return -1;
    }

    if(access_type == 0) {
        uint64_t value = vmexit_info->registers->r15;
        vmx_write(VMX_GUEST_CR3, value);
    } else if(access_type == 1) {
        vmexit_info->registers->r15 = vmexit_info->guest_cr3;
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled Control Register Access: 0x%llx", exit_qualification);
        return -1;
    }

    hypervisor_vmcs_goto_next_instruction(vmexit_info);

    return (uint64_t)vmexit_info->registers;
}

static uint64_t hypervisor_vmcs_exception_or_nmi_handler(vmcs_vmexit_info_t* exit_info) {
    uint64_t interrupt_info = exit_info->interrupt_info;

    int8_t vector = interrupt_info & 0xFF;

    if(vector == 0xE) {
        uint64_t ret = hypervisor_ept_page_fault_handler(exit_info);

        if(ret == -1ULL) {
            return -1;
        }

        return (uint64_t)exit_info->registers;
    }

    return -1;
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
        .guest_rflags = vmx_read(VMX_GUEST_RFLAGS),
        .guest_rip = vmx_read(VMX_GUEST_RIP),
        .guest_rsp = vmx_read(VMX_GUEST_RSP),
        .guest_efer = vmx_read(VMX_GUEST_IA32_EFER),
        .guest_cr0 = vmx_read(VMX_GUEST_CR0),
        .guest_cr3 = vmx_read(VMX_GUEST_CR3),
        .guest_cr4 = vmx_read(VMX_GUEST_CR4),
        .vm = task_get_vm(),
    };

    if (vmexit_info.reason < VMX_VMEXIT_REASON_COUNT) {
        if (vmexit_handlers[vmexit_info.reason]) {
            uint64_t ret = vmexit_handlers[vmexit_info.reason](&vmexit_info);

            if(ret == (uint64_t)registers || ret == 0) {
                return ret;
            }
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
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    RIP: 0x%016llx RFLAGS: 0x%08llx EFER: 0x%08llx",
             vmexit_info.guest_rip, vmexit_info.guest_rflags,
             vmexit_info.guest_efer);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    RAX: 0x%016llx RBX: 0x%016llx RCX: 0x%016llx RDX: 0x%016llx",
             vmexit_info.registers->rax, vmexit_info.registers->rbx,
             vmexit_info.registers->rcx, vmexit_info.registers->rdx);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    RSI: 0x%016llx RDI: 0x%016llx RBP: 0x%016llx RSP: 0x%016llx",
             vmexit_info.registers->rsi, vmexit_info.registers->rdi,
             vmexit_info.registers->rbp, vmexit_info.guest_rsp);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    R8:  0x%016llx R9:  0x%016llx R10: 0x%016llx R11: 0x%016llx",
             vmexit_info.registers->r8, vmexit_info.registers->r9,
             vmexit_info.registers->r10, vmexit_info.registers->r11);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    R12: 0x%016llx R13: 0x%016llx R14: 0x%016llx R15: 0x%016llx\n",
             vmexit_info.registers->r12, vmexit_info.registers->r13,
             vmexit_info.registers->r14, vmexit_info.registers->r15);
    PRINTLOG(HYPERVISOR, LOG_ERROR, "    CR0: 0x%08llx CR2: 0x%016llx CR3: 0x%016llx CR4: 0x%08llx\n",
             vmexit_info.guest_cr0, vmexit_info.registers->cr2,
             vmexit_info.guest_cr3, vmexit_info.guest_cr4);

    while(true) {
        cpu_idle();
    }
    return -1;
}

int8_t hypervisor_vmcs_prepare_vmexit_handlers(void) {
    vmexit_handlers[VMX_VMEXIT_REASON_EXTERNAL_INTERRUPT] = hypervisor_vmcs_external_interrupt_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_EPT_MISCONFIG] = hypervisor_vmcs_ept_misconfig_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_HLT] = hypervisor_vmcs_hlt_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_PAUSE] = hypervisor_vmcs_pause_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_IO_INSTRUCTION] = hypervisor_vmcs_io_instruction_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_INTERRUPT_WINDOW] = hypervisor_vmcs_interrupt_window_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_RDMSR] = hypervisor_vmcs_rdmsr_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_WRMSR] = hypervisor_vmcs_wrmsr_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_VMCALL] = hypervisor_vmcs_vmcalls_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_CONTROL_REGISTER_ACCESS] = hypervisor_vmcs_control_register_access_handler;
    vmexit_handlers[VMX_VMEXIT_REASON_EXCEPTION_OR_NMI] = hypervisor_vmcs_exception_or_nmi_handler;
    return 0;
}
