/**
 * @file hypervisor_vmx_ipc.64.c
 * @brief VMExit handler for 64-bit mode.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_vmx_vmcs_ops.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vmx_macros.h>

MODULE("turnstone.hypervisor.vmx");

int8_t hypervisor_vmx_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message) {
    vmx_vmcs_vmexit_info_t* vmexit_info = vm->extra_data;
    buffer_t* buffer = message->message_data;

    buffer_printf(buffer, "rax: 0x%016llx ", vmexit_info->registers->rax);
    buffer_printf(buffer, "rbx: 0x%016llx ", vmexit_info->registers->rbx);
    buffer_printf(buffer, "rcx: 0x%016llx ", vmexit_info->registers->rcx);
    buffer_printf(buffer, "rdx: 0x%016llx\n", vmexit_info->registers->rdx);
    buffer_printf(buffer, "rsi: 0x%016llx ", vmexit_info->registers->rsi);
    buffer_printf(buffer, "rdi: 0x%016llx ", vmexit_info->registers->rdi);
    buffer_printf(buffer, "rbp: 0x%016llx ", vmexit_info->registers->rbp);
    buffer_printf(buffer, "rsp: 0x%016llx\n", vmx_read(VMX_GUEST_RSP));
    buffer_printf(buffer, "r8:  0x%016llx ", vmexit_info->registers->r8);
    buffer_printf(buffer, "r9:  0x%016llx ", vmexit_info->registers->r9);
    buffer_printf(buffer, "r10: 0x%016llx ", vmexit_info->registers->r10);
    buffer_printf(buffer, "r11: 0x%016llx\n", vmexit_info->registers->r11);
    buffer_printf(buffer, "r12: 0x%016llx ", vmexit_info->registers->r12);
    buffer_printf(buffer, "r13: 0x%016llx ", vmexit_info->registers->r13);
    buffer_printf(buffer, "r14: 0x%016llx ", vmexit_info->registers->r14);
    buffer_printf(buffer, "r15: 0x%016llx\n", vmexit_info->registers->r15);
    buffer_printf(buffer, "rip: 0x%016llx rflags: 0x%08llx(0x%08llx) hlt=%i\n",
                  vmexit_info->guest_rip, vmexit_info->guest_rflags, vmexit_info->registers->rflags,
                  vm->is_halted);
    buffer_printf(buffer, "\n");

    buffer_printf(buffer, "cr0: 0x%08llx cr2: 0x%016llx cr3: 0x%016llx cr4: 0x%08llx\n",
                  vmexit_info->guest_cr0, vmexit_info->registers->cr2, vmexit_info->guest_cr3, vmexit_info->guest_cr4);
    buffer_printf(buffer, "\n");

    buffer_printf(buffer, "cs:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_CS_SELECTOR), vmx_read(VMX_GUEST_CS_BASE),
                  vmx_read(VMX_GUEST_CS_LIMIT), vmx_read(VMX_GUEST_CS_ACCESS_RIGHT));
    buffer_printf(buffer, "ss:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_SS_SELECTOR), vmx_read(VMX_GUEST_SS_BASE),
                  vmx_read(VMX_GUEST_SS_LIMIT), vmx_read(VMX_GUEST_SS_ACCESS_RIGHT));
    buffer_printf(buffer, "ds:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_DS_SELECTOR), vmx_read(VMX_GUEST_DS_BASE),
                  vmx_read(VMX_GUEST_DS_LIMIT), vmx_read(VMX_GUEST_DS_ACCESS_RIGHT));
    buffer_printf(buffer, "es:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_ES_SELECTOR), vmx_read(VMX_GUEST_ES_BASE),
                  vmx_read(VMX_GUEST_ES_LIMIT), vmx_read(VMX_GUEST_ES_ACCESS_RIGHT));
    buffer_printf(buffer, "fs:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_FS_SELECTOR), vmx_read(VMX_GUEST_FS_BASE),
                  vmx_read(VMX_GUEST_FS_LIMIT), vmx_read(VMX_GUEST_FS_ACCESS_RIGHT));
    buffer_printf(buffer, "gs:  0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_GS_SELECTOR), vmx_read(VMX_GUEST_GS_BASE),
                  vmx_read(VMX_GUEST_GS_LIMIT), vmx_read(VMX_GUEST_GS_ACCESS_RIGHT));
    buffer_printf(buffer, "\n");

    buffer_printf(buffer, "ldtr: 0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_LDTR_SELECTOR), vmx_read(VMX_GUEST_LDTR_BASE),
                  vmx_read(VMX_GUEST_LDTR_LIMIT), vmx_read(VMX_GUEST_LDTR_ACCESS_RIGHT));
    buffer_printf(buffer, "tr:   0x%04llx 0x%016llx 0x%08llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_TR_SELECTOR), vmx_read(VMX_GUEST_TR_BASE),
                  vmx_read(VMX_GUEST_TR_LIMIT), vmx_read(VMX_GUEST_TR_ACCESS_RIGHT));
    buffer_printf(buffer, "gdtr: 0x%016llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_GDTR_BASE), vmx_read(VMX_GUEST_GDTR_LIMIT));
    buffer_printf(buffer, "idt:  0x%016llx 0x%08llx\n",
                  vmx_read(VMX_GUEST_IDTR_BASE), vmx_read(VMX_GUEST_IDTR_LIMIT));
    buffer_printf(buffer, "efer: 0x%08llx\n",
                  vmexit_info->guest_efer);

    buffer_printf(buffer, "\n");
    buffer_printf(buffer, "lapic timer:\n");
    buffer_printf(buffer, "\tenabled: %s\n", (vm->lapic_timer_enabled == 0) ? "no" : "yes");
    buffer_printf(buffer, "\tcurrent: 0x%016llx\n", vm->lapic.timer_current_value);
    buffer_printf(buffer, "\tinitial: 0x%016llx\n", vm->lapic.timer_initial_value);
    buffer_printf(buffer, "\tdivide:  0x%016llx\n", vm->lapic.timer_divider);
    buffer_printf(buffer, "\tvector:  0x%02x type: %s masked: %s\n", vm->lapic.timer_vector,
                  (vm->lapic.timer_periodic == 0) ? "one-shot" : "periodic",
                  (vm->lapic.timer_masked == 0) ? "unmasked" : "masked");

    buffer_printf(buffer, "\n");

    buffer_printf(buffer, "lapic eoi pending: %s\n", (vm->lapic.apic_eoi_pending == 0) ? "no" : "yes");
    buffer_printf(buffer, "in service vector: 0x%02x\n", vm->lapic.in_service_vector);
    buffer_printf(buffer, "in request vectors: ");

    for(uint32_t vector = 0; vector < 256; vector++) {
        uint32_t vector_byte = vector / 64;
        uint32_t vector_bit = vector % 64;

        if(vm->lapic.in_request_vectors[vector_byte] & (1 << vector_bit)) {
            buffer_printf(buffer, "0x%02x ", vector);
        }
    }

    buffer_printf(buffer, "\n");

    message->message_data_completed = true;

    return 0;
}

int8_t hypervisor_vmx_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector) {
    vmx_vmcs_vmexit_info_t* vmexit_info = vm->extra_data;

    uint32_t vector_byte = vector / 64;
    uint32_t vector_bit = vector % 64;

    vm->lapic.in_request_vectors[vector_byte] |= (1 << vector_bit);

    vm->need_to_notify = true;

    uint64_t rflags = vmexit_info->guest_rflags;

    if(rflags & 0x200) {
        // enable interrupt window exiting if IF is set
        vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, vmx_read(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION) | (1 << 2));

        if(vm->is_halted) {
            vm->is_halt_need_next_instruction = true;
        }
    }

    return 0;
}


