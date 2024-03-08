/**
 * @file hypervisor_ipc.64.c
 * @brief Hypervisor Virtual Machine Management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_vmcsops.h>
#include <hypervisor/hypervisor_vmxops.h>
#include <hypervisor/hypervisor_macros.h>
#include <hypervisor/hypervisor_vm.h>
#include <list.h>
#include <cpu/task.h>
#include <memory.h>
#include <logging.h>
#include <time.h>

MODULE("turnstone.hypervisor");

extern list_t* hypervisor_vm_list;

static int8_t hypervisor_ipc_handle_dump(vmcs_vmexit_info_t* vmexit_info, hypervisor_ipc_message_t* message) {
    buffer_t* buffer = message->message_data;

    hypervisor_vm_t* vm = task_get_vm();

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
                  vmx_read(VMX_GUEST_RIP), vmx_read(VMX_GUEST_RFLAGS), vmexit_info->registers->rflags,
                  vm->is_halted);
    buffer_printf(buffer, "\n");

    buffer_printf(buffer, "cr0: 0x%08llx cr2: 0x%016llx cr3: 0x%016llx cr4: 0x%08llx\n",
                  vmx_read(VMX_GUEST_CR0), vmexit_info->registers->cr2, vmx_read(VMX_GUEST_CR3), vmx_read(VMX_GUEST_CR4));
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
                  vmx_read(VMX_GUEST_IA32_EFER));

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
        uint32_t vector_byte = vector / 8;
        uint32_t vector_bit = vector % 8;

        if(vm->lapic.in_request_vectors[vector_byte] & (1 << vector_bit)) {
            buffer_printf(buffer, "0x%02x ", vector);
        }
    }

    buffer_printf(buffer, "\n");

    message->message_data_completed = true;

    return 0;
}

static int8_t hypervisor_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector) {
    uint32_t vector_byte = vector / 8;
    uint32_t vector_bit = vector % 8;

    vm->lapic.in_request_vectors[vector_byte] |= (1 << vector_bit);

    vm->need_to_notify = true;

    uint64_t rflags = vmx_read(VMX_GUEST_RFLAGS);

    if(rflags & 0x200) {
        // enable interrupt window exiting if IF is set
        vmx_write(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION, vmx_read(VMX_CTLS_PRI_PROC_BASED_VM_EXECUTION) | (1 << 2));

        if(vm->is_halted) {
            vm->is_halt_need_next_instruction = true;
        }
    }

    return 0;
}

static int8_t hypervisor_ipc_handle_timer_int(vmcs_vmexit_info_t* vmexit_info, hypervisor_ipc_message_t* message) {
    UNUSED(vmexit_info);
    UNUSED(message);

    hypervisor_vm_t* vm = task_get_vm();

    if(!vm) {
        return -1;
    }

    if(vm->lapic.timer_masked) {
        return 0;
    }

    return hypervisor_ipc_handle_irq(vm, vm->lapic.timer_vector);
}


int8_t hypervisor_vmcs_check_ipc(vmcs_vmexit_info_t* vmexit_info) {
    list_t* mq = task_get_current_task_message_queue(0);

    if(!mq) {
        return -1;
    }

    while(list_size(mq)) {
        hypervisor_ipc_message_t* message = (hypervisor_ipc_message_t*)list_queue_pop(mq);

        if(!message) {
            return -1;
        }

        switch(message->message_type) {
        case HYPERVISOR_IPC_MESSAGE_TYPE_DUMP:
            hypervisor_ipc_handle_dump(vmexit_info, message);
            break;
        case HYPERVISOR_IPC_MESSAGE_TYPE_TIMER_INT:
            hypervisor_ipc_handle_timer_int(vmexit_info, message);
            break;
        case HYPERVISOR_IPC_MESSAGE_TYPE_CLOSE:
            task_end_task();
            break;
        default:
            break;
        }
    }

    return 0;
}

const hypervisor_ipc_message_t hypervisor_ipc_message_timer_int = {
    .message_type = HYPERVISOR_IPC_MESSAGE_TYPE_TIMER_INT,
};

void hypervisor_ipc_send_timer_interrupt(hypervisor_vm_t* vm) {
    if(!vm) {
        return;
    }

    list_t* mq = vm->ipc_queue;

    if(!mq) {
        return;
    }

    list_queue_push(mq, (void*)&hypervisor_ipc_message_timer_int);
}

const hypervisor_ipc_message_t hypervisor_ipc_message_close = {
    .message_type = HYPERVISOR_IPC_MESSAGE_TYPE_CLOSE,
};

int8_t hypervisor_ipc_send_close(uint64_t vm_id) {
    list_t* vm_mq = task_get_message_queue(vm_id, 0);

    if(!vm_mq) {
        printf("VM not found\n");
        return -1;
    }

    list_queue_push(vm_mq, (void*)&hypervisor_ipc_message_timer_int);

    return 0;
}
