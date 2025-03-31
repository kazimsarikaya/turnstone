/**
 * @file hypervisor_ipc.64.c
 * @brief Hypervisor Virtual Machine Management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_ipc.h>
#include <hypervisor/hypervisor_vmx_vmcs_ops.h>
#include <hypervisor/hypervisor_vmx_ops.h>
#include <hypervisor/hypervisor_vmx_macros.h>
#include <hypervisor/hypervisor_vm.h>
#include <list.h>
#include <cpu.h>
#include <cpu/task.h>
#include <memory.h>
#include <logging.h>
#include <time.h>

MODULE("turnstone.hypervisor.ipc");

static int8_t hypervisor_ipc_handle_irq(hypervisor_vm_t* vm, uint8_t vector) {
    if(cpu_get_type() == CPU_TYPE_INTEL) {
        return hypervisor_vmx_ipc_handle_irq(vm, vector);
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        return hypervisor_svm_ipc_handle_irq(vm, vector);
    }
    PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
    return -1;
}

static int8_t hypervisor_ipc_handle_timer_int(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message) {
    UNUSED(message);

    if(!vm) {
        return -1;
    }

    if(vm->lapic.timer_masked) {
        return 0;
    }

    return hypervisor_ipc_handle_irq(vm, vm->lapic.timer_vector);
}

static void hypervisor_ipc_handle_interrupts(hypervisor_vm_t* vm) {
    if(!vm) {
        return;
    }

    list_t* mq = vm->interrupt_queue;

    if(list_size(mq)) {
        interrupt_frame_ext_t* frame = (interrupt_frame_ext_t*)list_queue_peek(mq);

        hypervisor_ipc_handle_irq(vm, frame->interrupt_number);
    }
}

static void hypervisor_ipc_handle_dump(hypervisor_vm_t* vm, hypervisor_ipc_message_t* message) {
    if(cpu_get_type() == CPU_TYPE_INTEL) {
        hypervisor_vmx_ipc_handle_dump(vm, message);
    } else if(cpu_get_type() == CPU_TYPE_AMD) {
        hypervisor_svm_ipc_handle_dump(vm, message);
    } else {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "Hypervisor not supported");
    }
}

int8_t hypervisor_check_ipc(hypervisor_vm_t* vm) {
    hypervisor_ipc_handle_interrupts(vm);

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
            hypervisor_ipc_handle_dump(vm, message);
            break;
        case HYPERVISOR_IPC_MESSAGE_TYPE_TIMER_INT:
            hypervisor_ipc_handle_timer_int(vm, message);
            break;
        case HYPERVISOR_IPC_MESSAGE_TYPE_CLOSE:
            PRINTLOG(HYPERVISOR, LOG_INFO, "VM close message received. Close sequence started");
            return -2; // close
            break;
        default:
            PRINTLOG(HYPERVISOR, LOG_ERROR, "Invalid IPC message type: 0x%x", message->message_type);
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
        printf("VM not found: 0x%llx\n", vm_id);
        return -1;
    }

    list_queue_push(vm_mq, (void*)&hypervisor_ipc_message_close);

    return 0;
}
