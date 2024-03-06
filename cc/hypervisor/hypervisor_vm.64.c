/**
 * @file hypervisor_vm.64.c
 * @brief Hypervisor Virtual Machine Management
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#include <hypervisor/hypervisor_vm.h>
#include <hypervisor/hypervisor_ipc.h>
#include <list.h>
#include <cpu/task.h>
#include <memory.h>
#include <logging.h>
#include <time.h>

MODULE("turnstone.hypervisor");

list_t* hypervisor_vm_list = NULL;

extern volatile uint64_t time_timer_rdtsc_delta;

int8_t hypervisor_vm_init(void) {
    if (hypervisor_vm_list != NULL) {
        return 0;
    }

    hypervisor_vm_list = list_create_list();

    if (hypervisor_vm_list == NULL) {
        return -1;
    }

    return 0;
}

int8_t hypervisor_vm_create_and_attach_to_task(uint64_t vmcs_frame_fa) {
    if (hypervisor_vm_list == NULL) {
        return -1;
    }

    list_t* mq_list = list_create_queue();

    if(mq_list == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create message queue");
        return -1;
    }

    task_add_message_queue(mq_list);

    buffer_t* buffer = buffer_new();

    if(buffer == NULL) {
        PRINTLOG(HYPERVISOR, LOG_ERROR, "cannot create buffer");
        return -1;
    }

    task_set_output_buffer(buffer);

    hypervisor_vm_t* vm = memory_malloc(sizeof(hypervisor_vm_t));

    if (vm == NULL) {
        return -1;
    }

    vm->heap = memory_get_heap(NULL);
    vm->vmcs_frame_fa = vmcs_frame_fa;
    vm->ipc_queue = mq_list;
    vm->task_id = task_get_id();
    vm->last_tsc = rdtsc();
    vm->output_buffer = buffer;
    vm->msr_map = map_integer();

    list_list_insert(hypervisor_vm_list, vm);

    PRINTLOG(HYPERVISOR, LOG_DEBUG, "vmcs frame fa: 0x%llx", vmcs_frame_fa);
    task_set_vmcs_physical_address(vmcs_frame_fa);
    task_set_vm(vm);

    return 0;
}

void hypervisor_vm_destroy(hypervisor_vm_t* vm) {
    if (vm == NULL) {
        return;
    }

    list_list_delete(hypervisor_vm_list, vm);

    buffer_destroy(vm->output_buffer);
    list_destroy(vm->ipc_queue);
    map_destroy(vm->msr_map);

    memory_free_ext(vm->heap, vm);
}

void hypervisor_vm_notify_timers(void) {
    if (hypervisor_vm_list == NULL) {
        return;
    }

    for(uint64_t i = 0; i < list_size(hypervisor_vm_list); i++) {
        hypervisor_vm_t* vm = (hypervisor_vm_t*)list_get_data_at_position(hypervisor_vm_list, i);

        if(vm == NULL) {
            continue;
        }

        if(!vm->lapic_timer_enabled) {
            continue;
        }

        uint64_t tsc = rdtsc();

        uint64_t delta = tsc - vm->last_tsc;
        delta /= time_timer_rdtsc_delta;
        delta *= vm->lapic.timer_divider_realvalue;

        vm->last_tsc = tsc;

        boolean_t timer_expired = false;

        if(vm->lapic.timer_current_value > delta) {
            vm->lapic.timer_current_value -= delta; // delta;
        } else {
            vm->lapic.timer_current_value = vm->lapic.timer_initial_value;
            timer_expired = true;
        }

        if(timer_expired && !vm->lapic.timer_masked && !vm->lapic_timer_pending) {
            vm->lapic_timer_pending = true;
            hypervisor_ipc_send_timer_interrupt(vm);
        }
    }
}
