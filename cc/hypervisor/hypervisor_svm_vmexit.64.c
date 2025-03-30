/**
 * @file hypervisor_svm_vmexit.64.c
 * @brief SVM VMEXIT handler for 64-bit x86 architecture.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_svm_vmcb_ops.h>
#include <hypervisor/hypervisor_svm_macros.h>
#include <hypervisor/hypervisor_utils.h>
#include <hypervisor/hypervisor_ept.h>
#include <hypervisor/hypervisor_ipc.h>
#include <memory/paging.h>
#include <cpu.h>
#include <cpu/task.h>
#include <apic.h>
#include <cpu/crx.h>
#include <logging.h>
#include <apic.h>
#include <ports.h>
#include <strings.h>

MODULE("turnstone.hypervisor.svm");

__attribute__((naked, no_stack_protector)) static void hypervisor_svm_vm_run_single(
    task_registers_t* host_registers,
    task_registers_t* guest_registers,
    uint64_t          vmcb_frame_fa) {
    UNUSED(vmcb_frame_fa);
    asm volatile (
        "push %%rbp\n"
        "mov %%rsp, %%rbp\n"
        "// create 32 bytes for local variables\n"
        "// 8 bytes for host registers\n"
        "// 8 bytes for guest registers\n"
        "// 8 bytes for vmcb frame fa\n"
        "sub $0x20, %%rsp\n"
        "mov %%rdi, 0x0(%%rsp)\n"
        "mov %%rsi, 0x8(%%rsp)\n"
        "mov %%rdx, 0x10(%%rsp)\n"


        "// begin save host registers\n"
        "mov %%rbx, %[h_rbx]\n"
        "mov %%rcx, %[h_rcx]\n"
        "mov %%rdx, %[h_rdx]\n"
        "mov %%r8,  %[h_r8]\n"
        "mov %%r9,  %[h_r9]\n"
        "mov %%r10, %[h_r10]\n"
        "mov %%r11, %[h_r11]\n"
        "mov %%r12, %[h_r12]\n"
        "mov %%r13, %[h_r13]\n"
        "mov %%r14, %[h_r14]\n"
        "mov %%r15, %[h_r15]\n"
        "mov %%rdi, %[h_rdi]\n"
        "mov %%rsi, %[h_rsi]\n"
        "mov %%rbp, %[h_rbp]\n"
        "mov %[h_xsave_mask_lo], %%eax\n"
        "mov %[h_xsave_mask_hi], %%edx\n"
        "lea %[h_avx512f], %%rbx\n"
        "xsave (%%rbx)\n"
        "// end save host registers\n"
        : :
        [h_rbx]    "m" (host_registers->rbx),
        [h_rcx]    "m" (host_registers->rcx),
        [h_rdx]    "m" (host_registers->rdx),
        [h_r8]     "m" (host_registers->r8),
        [h_r9]     "m" (host_registers->r9),
        [h_r10]    "m" (host_registers->r10),
        [h_r11]    "m" (host_registers->r11),
        [h_r12]    "m" (host_registers->r12),
        [h_r13]    "m" (host_registers->r13),
        [h_r14]    "m" (host_registers->r14),
        [h_r15]    "m" (host_registers->r15),
        [h_rdi]    "m" (host_registers->rdi),
        [h_rsi]    "m" (host_registers->rsi),
        [h_rbp]    "m" (host_registers->rbp),
        [h_avx512f]    "m" (host_registers->avx512f),
        [h_xsave_mask_lo] "m" (host_registers->xsave_mask_lo),
        [h_xsave_mask_hi] "m" (host_registers->xsave_mask_hi)
        );

    asm volatile (
        "// begin save host vmcb frame\n"
        "mov 0x10(%rsp), %rax\n"
        "addq $0x1000, %rax\n"
        "vmsave\n"
        "// end save host vmcb frame\n"
        );

    asm volatile (
        "// begin load guest registers\n"
        "mov %[g_rcx], %%rcx\n"
        "mov %[g_r8],  %%r8\n"
        "mov %[g_r9],  %%r9\n"
        "mov %[g_r10], %%r10\n"
        "mov %[g_r11], %%r11\n"
        "mov %[g_r12], %%r12\n"
        "mov %[g_r13], %%r13\n"
        "mov %[g_r14], %%r14\n"
        "mov %[g_r15], %%r15\n"
        "mov %[g_rdi], %%rdi\n"
        "mov %[g_rbp], %%rbp\n"
        "lea %[g_avx512f], %%rbx\n"
        "mov %[g_xsave_mask_lo], %%eax\n"
        "mov %[g_xsave_mask_hi], %%edx\n"
        "xrstor (%%rbx)\n"
        "mov %[g_rbx], %%rbx\n"
        "mov %[g_rdx], %%rdx\n"
        "mov %[g_rsi], %%rsi\n"
        "// end load guest registers\n"
        : :
        [g_rbx]    "m" (guest_registers->rbx),
        [g_rcx]    "m" (guest_registers->rcx),
        [g_rdx]    "m" (guest_registers->rdx),
        [g_r8]     "m" (guest_registers->r8),
        [g_r9]     "m" (guest_registers->r9),
        [g_r10]    "m" (guest_registers->r10),
        [g_r11]    "m" (guest_registers->r11),
        [g_r12]    "m" (guest_registers->r12),
        [g_r13]    "m" (guest_registers->r13),
        [g_r14]    "m" (guest_registers->r14),
        [g_r15]    "m" (guest_registers->r15),
        [g_rdi]    "m" (guest_registers->rdi),
        [g_rsi]    "m" (guest_registers->rsi),
        [g_rbp]    "m" (guest_registers->rbp),
        [g_avx512f]    "m" (guest_registers->avx512f),
        [g_xsave_mask_lo] "m" (guest_registers->xsave_mask_lo),
        [g_xsave_mask_hi] "m" (guest_registers->xsave_mask_hi)
        );

    asm volatile (
        "// begin load guest vmcb frame\n"
        "mov 0x10(%rsp), %rax\n"
        "vmload\n"
        "// end load guest vmcb frame\n"

        "// begin vmrun\n"
        "vmrun\n"
        "// end vmrun\n"

        "// begin save guest vmcb frame\n"
        "vmsave\n"
        "// end save guest vmcb frame\n"
        );

    asm volatile (
        "// now tricky part we need to save guest registers however rsi is guest rsi\n"
        "xchg 0x8(%%rsp), %%rsi\n"

        "// begin save guest registers\n"
        "mov %%rbx, %[g_rbx]\n"
        "mov %%rcx, %[g_rcx]\n"
        "mov %%rdx, %[g_rdx]\n"
        "mov %%r8,  %[g_r8]\n"
        "mov %%r9,  %[g_r9]\n"
        "mov %%r10, %[g_r10]\n"
        "mov %%r11, %[g_r11]\n"
        "mov %%r12, %[g_r12]\n"
        "mov %%r13, %[g_r13]\n"
        "mov %%r14, %[g_r14]\n"
        "mov %%r15, %[g_r15]\n"
        "mov %%rdi, %[g_rdi]\n"
        "mov %%rbp, %[g_rbp]\n"
        "mov %[g_xsave_mask_lo], %%eax\n"
        "mov %[g_xsave_mask_hi], %%edx\n"
        "lea %[g_avx512f], %%rbx\n"
        "xsave (%%rbx)\n"
        "// move guest rsi to rax\n"
        "mov 0x8(%%rsp), %%rax\n"
        "// and save it\n"
        "mov %%rax, %[g_rsi]\n"
        "// end save guest registers\n"
        : :
        [g_rbx]    "m" (guest_registers->rbx),
        [g_rcx]    "m" (guest_registers->rcx),
        [g_rdx]    "m" (guest_registers->rdx),
        [g_r8]     "m" (guest_registers->r8),
        [g_r9]     "m" (guest_registers->r9),
        [g_r10]    "m" (guest_registers->r10),
        [g_r11]    "m" (guest_registers->r11),
        [g_r12]    "m" (guest_registers->r12),
        [g_r13]    "m" (guest_registers->r13),
        [g_r14]    "m" (guest_registers->r14),
        [g_r15]    "m" (guest_registers->r15),
        [g_rdi]    "m" (guest_registers->rdi),
        [g_rsi]    "m" (guest_registers->rsi),
        [g_rbp]    "m" (guest_registers->rbp),
        [g_avx512f]    "m" (guest_registers->avx512f),
        [g_xsave_mask_lo] "m" (guest_registers->xsave_mask_lo),
        [g_xsave_mask_hi] "m" (guest_registers->xsave_mask_hi)
        );

    asm volatile (
        "// now restore host parametes rsi is already host rsi\n"
        "mov 0x0(%%rsp), %%rdi\n"

        "// begin restore host registers\n"
        "mov %[h_rcx],  %%rcx\n"
        "mov %[h_r8],   %%r8\n"
        "mov %[h_r9],   %%r9\n"
        "mov %[h_r10],  %%r10\n"
        "mov %[h_r11],  %%r11\n"
        "mov %[h_r12],  %%r12\n"
        "mov %[h_r13],  %%r13\n"
        "mov %[h_r14],  %%r14\n"
        "mov %[h_r15],  %%r15\n"
        "mov %[h_rdi],  %%rdi\n"
        "mov %[h_rsi],  %%rsi\n"
        "mov %[h_rbp],  %%rbp\n"
        "mov %[h_xsave_mask_lo], %%eax\n"
        "mov %[h_xsave_mask_hi], %%edx\n"
        "lea %[h_avx512f], %%rbx\n"
        "xrstor (%%rbx)\n"
        "mov %[h_rbx],  %%rbx\n"
        "mov %[h_rdx],  %%rdx\n"
        "// end restore host registers\n"

        "mov 0x10(%%rsp), %%rax\n"
        "addq $0x1000, %%rax\n"
        "vmload\n"
        "add $0x20, %%rsp\n"
        "pop %%rbp\n"
        "retq\n"
        : :
        [h_rbx]    "m" (host_registers->rbx),
        [h_rcx]    "m" (host_registers->rcx),
        [h_rdx]    "m" (host_registers->rdx),
        [h_r8]     "m" (host_registers->r8),
        [h_r9]     "m" (host_registers->r9),
        [h_r10]    "m" (host_registers->r10),
        [h_r11]    "m" (host_registers->r11),
        [h_r12]    "m" (host_registers->r12),
        [h_r13]    "m" (host_registers->r13),
        [h_r14]    "m" (host_registers->r14),
        [h_r15]    "m" (host_registers->r15),
        [h_rdi]    "m" (host_registers->rdi),
        [h_rsi]    "m" (host_registers->rsi),
        [h_rbp]    "m" (host_registers->rbp),
        [h_avx512f]    "m" (host_registers->avx512f),
        [h_xsave_mask_lo] "m" (host_registers->xsave_mask_lo),
        [h_xsave_mask_hi] "m" (host_registers->xsave_mask_hi)
        );
}

static void hypervisor_svm_wait_idle(void) {
    while(true) {
        asm volatile ("hlt");
    }
}

typedef int8_t (*hypervisor_svm_vmexit_handler_f)(hypervisor_vm_t* vm);

static void hypervisor_svm_goto_next_instruction(hypervisor_vm_t* vm) {
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    vmcb->save_state_area.rip = vmcb->control_area.n_rip;
}

static int8_t hypervisor_svm_vmexit_handler_intr(hypervisor_vm_t* vm) { // external interrupt
    UNUSED(vm);

    int32_t interrupt_vector = apic_get_isr_interrupt();

    if(interrupt_vector == -1) {
        return 0;
    }

    interrupt_frame_ext_t* frame = memory_malloc(sizeof(interrupt_frame_ext_t));

    if(!frame) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Cannot allocate memory for interrupt frame");
        return -1;
    }

    frame->interrupt_number = interrupt_vector;

    cpu_cli();

    interrupt_generic_handler(frame);

    cpu_sti();

    memory_free(frame);

    return 0;
}

static int8_t hypervisor_svm_vmexit_handler_hlt(hypervisor_vm_t* vm) { // halt
    vm->is_halted = true;

    task_set_message_waiting();

    task_yield();

    hypervisor_check_ipc(vm);

    if(vm->is_halt_need_next_instruction) {
        vm->is_halted = false;
        vm->is_halt_need_next_instruction = false;
        hypervisor_svm_goto_next_instruction(vm);
    }

    return 0;
}

static int8_t hypervisor_svm_vmexit_handler_pause(hypervisor_vm_t* vm) { // pause
    vm->is_halted = true;

    task_yield();

    hypervisor_check_ipc(vm);

    vm->is_halted = false;

    hypervisor_svm_goto_next_instruction(vm);

    return 0;
}

static int8_t hypervisor_svm_vmexit_handler_vmmcall(hypervisor_vm_t* vm) {
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    uint64_t rax = vmcb->save_state_area.rax;

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcall rax 0x%llx", rax);

    uint64_t ret = -1;

    switch(rax) {
    case HYPERVISOR_VMCALL_NUMBER_EXIT:
        hypervisor_cleanup_mapped_interrupts(vm);

        task_end_task();

        hypervisor_svm_wait_idle(); // should never return

        break;
    case HYPERVISOR_VMCALL_NUMBER_GET_HOST_PHYSICAL_ADDRESS:
        ret = hypervisor_ept_guest_virtual_to_host_physical(vm, vm->guest_registers->rdi);

        break;
    case HYPERVISOR_VMCALL_NUMBER_ATTACH_PCI_DEV:
        ret = hypervisor_attach_pci_dev(vm, vm->guest_registers->rdi);
        vmcb->control_area.clean_bits.fields.np = 1;

        break;
    case HYPERVISOR_VMCALL_NUMBER_ATTACH_INTERRUPT: {
        uint64_t pci_dev_address = vm->guest_registers->rdi;
        vm_guest_interrupt_type_t interrupt_type = (vm_guest_interrupt_type_t)vm->guest_registers->rsi;
        uint8_t interrupt_number = (uint8_t)vm->guest_registers->rdx;

        ret = hypervisor_attach_interrupt(vm, pci_dev_address, interrupt_type, interrupt_number);

        break;

    }
    case HYPERVISOR_VMCALL_NUMBER_LOAD_MODULE: {
        uint64_t got_entry_address = vm->guest_registers->r11;
        ret = hypervisor_load_module(vm, got_entry_address);
        vmcb->control_area.clean_bits.fields.np = 1;

        break;
    }
    default:
        PRINTLOG(HYPERVISOR, LOG_ERROR, "unknown vmcall rax 0x%llx", rax);

        break;
    }

    vmcb->save_state_area.rax = ret;

    hypervisor_svm_goto_next_instruction(vm);

    return 0;
}

static void hypervisor_svm_io_fast_string_printf_io(hypervisor_vm_t* vm) {
    uint64_t rsi = vm->guest_registers->rsi;
    uint64_t rcx = vm->guest_registers->rcx;

    uint64_t port = 0x3f8;

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

    vm->guest_registers->rsi += data_len;
    vm->guest_registers->rcx -= data_len;;
}

static int8_t hypervisor_svm_vmexit_handler_ioio(hypervisor_vm_t* vm) {
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);

    svm_exit_ioio_t ioio = (svm_exit_ioio_t)vmcb->control_area.exit_info_1;

    uint8_t size = -1;

    if(ioio.fields.sz8) {
        size = 1;
    } else if(ioio.fields.sz16) {
        size = 2;
    } else if(ioio.fields.sz32) {
        size = 4;
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Invalid IO Instruction Size: 0x%x", size);
        return -1;
    }

    uint64_t count = 1;

    if(ioio.fields.rep) {
        count = vm->guest_registers->rcx;
    }

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Port: 0x%x, Size: 0x%x, Direction: 0x%x String %i Rep %i",
             ioio.fields.port, size, ioio.fields.type, ioio.fields.str, ioio.fields.rep);

    uint64_t mask = 0xFFFFFFFFFFFFFFFF >> (64 - (size * 8));

    uint64_t data = vmcb->save_state_area.rax;

    PRINTLOG(HYPERVISOR, LOG_TRACE, "IO Instruction: Data: 0x%llx mask 0x%llx", data, mask);

    data &= mask;

    boolean_t decrement = (vmcb->save_state_area.rflags >> 10) & 0x1;

    uint64_t data_ptr_fa = 0;
    uint64_t data_ptr_va = 0;

    if(ioio.fields.str) {
        if(ioio.fields.type == 0){ // out from rsi
            data_ptr_fa = hypervisor_ept_guest_virtual_to_host_physical(vm, vm->guest_registers->rsi);
            data_ptr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_ptr_fa);
        } else {
            data_ptr_fa = hypervisor_ept_guest_virtual_to_host_physical(vm, vm->guest_registers->rdi);
            data_ptr_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(data_ptr_fa);
        }
    }

    if(list_contains(vm->mapped_io_ports, (void*)(uint64_t)(ioio.fields.port)) == 0){
        for(uint64_t i = 0; i < count; i++) {
            if(ioio.fields.type == 0) {
                if(ioio.fields.str) {
                    void* data_ptr = (void*)data_ptr_va;

                    if(size == 1) {
                        data = *((uint8_t*)data_ptr);
                    } else if(size == 2) {
                        data = *((uint16_t*)data_ptr);
                    } else {
                        data = *((uint32_t*)data_ptr);
                    }

                    if(decrement) {
                        data_ptr_va -= size;
                    } else {
                        data_ptr_va += size;
                    }
                }


                if(size == 1) {
                    outb(ioio.fields.port, data);
                } else if(size == 2) {
                    outw(ioio.fields.port, data);
                } else {
                    outl(ioio.fields.port, data);
                }
            } else {
                uint64_t value = 0;

                if(size == 1) {
                    value = inb(ioio.fields.port);
                } else if(size == 2) {
                    value = inw(ioio.fields.port);
                } else {
                    value = inl(ioio.fields.port);
                }

                if(ioio.fields.str) {
                    void* data_ptr = (void*)data_ptr_va;

                    if(size == 1) {
                        *((uint8_t*)data_ptr) = value;
                    } else if(size == 2) {
                        *((uint16_t*)data_ptr) = value;
                    } else {
                        *((uint32_t*)data_ptr) = value;
                    }

                    if(decrement) {
                        data_ptr_va -= size;
                    } else {
                        data_ptr_va += size;
                    }

                } else {
                    vmcb->save_state_area.rax = (vmcb->save_state_area.rax & ~mask) | (value & mask);
                }
            }
        }

        if(ioio.fields.rep) {
            vm->guest_registers->rcx = 0;
        }

        if(ioio.fields.str) {
            if(decrement) {
                if(ioio.fields.type == 0) {
                    vm->guest_registers->rsi -= count * size;
                } else {
                    vm->guest_registers->rdi -= count * size;
                }
            } else {
                if(ioio.fields.type == 0) {
                    vm->guest_registers->rsi += count * size;
                } else {
                    vm->guest_registers->rdi += count * size;
                }
            }
        }
    } else {

        if(ioio.fields.port == 0x3f8 && ioio.fields.type == 0) {
            if(ioio.fields.str && ioio.fields.rep) {
                if(!decrement) {
                    hypervisor_svm_io_fast_string_printf_io(vm);
                } else {
                    // TODO: Implement decrement
                }
            } else {
                uint8_t* data_ptr = (uint8_t*)&data;
                for(uint8_t i = 0; i < size; i++) {
                    printf("%c", data_ptr[i]);
                }
            }
        } else {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Unhandled IO Instruction: Port: 0x%x, Size: 0x%x, Direction: 0x%x Data 0x%llx",
                     ioio.fields.port, size, ioio.fields.type, data);
            return -1;
        }
    }

    hypervisor_svm_goto_next_instruction(vm);

    return 0;
}

static int8_t hypervisor_svm_vmexit_handler_excp14(hypervisor_vm_t* vm) { // page fault
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(vm->vmcb_frame_fa);
    uint64_t error_code = vmcb->control_area.exit_info_1;
    uint64_t address = vmcb->control_area.exit_info_2;

    uint64_t ret = hypervisor_ept_page_fault_handler(NULL, error_code, address);

    if(ret == -1ULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "page fault handler failed");
        return -1;
    }

    vmcb->control_area.clean_bits.fields.np = 1;

    return 0;
}


hypervisor_svm_vmexit_handler_f hypervisor_svm_vmexit_handlers[SVM_VMEXIT_REASON_ARRAY_SIZE] = {
    [SVM_VMEXIT_REASON_INTR] = hypervisor_svm_vmexit_handler_intr,
    [SVM_VMEXIT_REASON_HLT] = hypervisor_svm_vmexit_handler_hlt,
    [SVM_VMEXIT_REASON_PAUSE] = hypervisor_svm_vmexit_handler_pause,
    [SVM_VMEXIT_REASON_VMMCALL] = hypervisor_svm_vmexit_handler_vmmcall,
    [SVM_VMEXIT_REASON_IOIO] = hypervisor_svm_vmexit_handler_ioio,
    [SVM_VMEXIT_REASON_EXCP14] = hypervisor_svm_vmexit_handler_excp14,
};

static uint64_t hypervisor_svm_vmexit_remap_exit_code(uint64_t exit_code) {
    if(exit_code <= SVM_VMEXIT_REASON_IDLE_HLT) {
        return exit_code;
    }

    uint32_t narrowed_exit_code = exit_code & 0xFFFFFFFF;

    switch(narrowed_exit_code) {
    case SVM_VMEXIT_REASON_NPF:
        return SVM_VMEXIT_REASON_NPF_REMAPPED;
    case SVM_VMEXIT_REASON_AVIC_INCOMPLETE_IPI:
        return SVM_VMEXIT_REASON_AVIC_INCOMPLETE_IPI_REMAPPED;
    case SVM_VMEXIT_REASON_AVIC_NOACCEL:
        return SVM_VMEXIT_REASON_AVIC_NOACCEL_REMAPPED;
    case SVM_VMEXIT_REASON_VMGEXIT:
        return SVM_VMEXIT_REASON_VMGEXIT_REMAPPED;
    case SVM_VMEXIT_REASON_INVALID:
        return SVM_VMEXIT_REASON_INVALID_REMAPPED;
    case SVM_VMEXIT_REASON_BUSY:
        return SVM_VMEXIT_REASON_BUSY_REMAPPED;
    case SVM_VMEXIT_REASON_IDLE_REQUIRED:
        return SVM_VMEXIT_REASON_IDLE_REQUIRED_REMAPPED;
    case SVM_VMEXIT_REASON_INVALID_PMC:
        return SVM_VMEXIT_REASON_INVALID_PMC_REMAPPED;
    case SVM_VMEXIT_REASON_UNUSED:
        return SVM_VMEXIT_REASON_UNUSED_REMAPPED;
    default:
        return SVM_VMEXIT_REASON_INVALID_REMAPPED;
    }

    return SVM_VMEXIT_REASON_INVALID_REMAPPED;
}

int8_t hypervisor_svm_vm_run(uint64_t hypervisor_vm_ptr) {
    hypervisor_vm_t* vm = (hypervisor_vm_t*)hypervisor_vm_ptr;

    if(vm == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "invalid vm");
        return -1;
    }

    uint64_t guest_vmcb = vm->vmcb_frame_fa;
    svm_vmcb_t* vmcb = (svm_vmcb_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(guest_vmcb);

    while(true) {
        asm volatile ("clgi");

        if(hypervisor_svm_vmcb_set_running(vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot set running");
            return -1;
        }

        hypervisor_svm_vm_run_single(vm->host_registers, vm->guest_registers, guest_vmcb);

        if(hypervisor_svm_vmcb_set_stopped(vm) != 0) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot set stopped");
            return -1;
        }

        asm volatile ("stgi");

        uint32_t exit_code = vmcb->control_area.exit_code;

        exit_code = hypervisor_svm_vmexit_remap_exit_code(exit_code);

        hypervisor_svm_vmexit_handler_f handler = hypervisor_svm_vmexit_handlers[exit_code];

        if(handler == NULL) {
            PRINTLOG(HYPERVISOR, LOG_ERROR, "unhandled vmexit occurred exit code: 0x%llx 0x%llx 0x%llx 0x%llx",
                     vmcb->control_area.exit_code, vmcb->control_area.exit_info_1, vmcb->control_area.exit_info_2, vmcb->control_area.exit_int_info.bits);

            hypervisor_svm_wait_idle();
        } else {
            if(handler(vm) != 0) {
                PRINTLOG(HYPERVISOR, LOG_ERROR, "vmexit handler failed");

                hypervisor_svm_wait_idle();
            }
        }
    }

    return 0;
}
